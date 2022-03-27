#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <chrono>
#include <vector>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <functional>

#include "stores.h"
#include "utils.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "tests.cpp"

namespace fs = std::filesystem;
using fs::path;
namespace chrono = std::chrono;
using namespace std::string_literals;
using std::string, std::to_string, std::vector, std::pair, std::function;
using utils::Range, utils::Stats, stores::Store, utils::KiB, utils::MiB, utils::GiB;
using StorePtr = std::unique_ptr<stores::Store>;

struct BenchmarkData {
    /** Which storage method */
    string store;
    /** One of "insert", "update", "get", "remove", "space", "memory" */
    string op;
    /** Size range in bytes */
    Range<size_t> size;
    /** Record count range */
    Range<size_t> records;
    /** One of "compressible", "incompressible" */
    string dataType;
    /**
     * The measurements taken. Units depend on the op:
     * `insert`, `update`, `get`, `remove`: nanoseconds
     * `space`: percent (space efficiency)
     * `memory`: kilobytes (peak memory usage)
     */
    Stats<long long> stats{};
};

/** A callable that generates random data for use as a value in the store */
using DataGenerator = function<string(Range<size_t>)>;

/** This class runs the actual benchmark */
class Benchmark {
public:
    /** Directory to save the stores */
    path storeDir;

    /** How many iterations of each measurement to do */
    const int repeats;

    /** Size ranges to test [min, max] */
    const vector<Range<size_t>> sizeRanges;

    /** Record count ranges to test [min, max] */
    const vector<Range<size_t>> countRanges;

    /** Incompressible vs compressible data */
    const vector<pair<string, DataGenerator>> dataTypes;


    /** Picks a random key from the store */
    string pickKey(const StorePtr& store) const {
        return utils::genKey(utils::randInt<size_t>(0, store->count() - 1));
    }

    StorePtr initStore(stores::Type type, size_t recordCount, Range<size_t> sizeRange, DataGenerator dataGen) {
        StorePtr store = getStore(type, storeDir / stores::types.at(type));
        for (size_t i = 0; i < recordCount; i++) {
            store->insert(utils::genKey(store->count()), dataGen(sizeRange));
        }
        return store;
    }

    /** Iterates over the store and gets the total data size on disk. */
    size_t getDataSize(StorePtr& store) {
        // probably could keep a running count as we make it instead, but we'd have to get to measure size before
        // update/remove (which could cause complications with caching). Or we'd need to close the database after
        // inserts so we can measure size on disk and then reopen or regenerate it to benchmark update/remove.
        size_t dataSize = 0;
        for (size_t i = 0; i < store->count(); i++) {
            dataSize += store->get(utils::genKey(i)).size();
        }
        return dataSize;
    }

    inline static const string CSV_HEADER = "store,op,size,records,data type,measurements,sum,min,max,avg\n";
    static string toCSVRow(const BenchmarkData& data) {
        return data.store + "," +
            data.op + "," +
            utils::prettySize(data.size.min) + " to " + utils::prettySize(data.size.max + 1) + "," +
            to_string(data.records.min) + " to " + to_string(data.records.max) + "," +
            data.dataType + "," +
            to_string(data.stats.count()) + "," +
            to_string(data.stats.sum()) + "," +
            to_string(data.stats.min()) + "," +
            to_string(data.stats.max()) + "," +
            to_string(data.stats.avg()) + "\n";
    }

    /** Runs the benchmark. Pass the output stream to save CSV data to */
    void run(std::ostream& output) {
        fs::remove_all(storeDir); // clear the storeDir
        fs::create_directories(storeDir);
        output << CSV_HEADER;

        utils::resetPeakMemUsage();
        size_t baseMemUsage = utils::getPeakMemUsage(); // We'll subtract the base from future measurements

        for (auto [type, typeName] : stores::types)
        for (auto [dataType, dataGen] : dataTypes)
        for (auto sizeRange : sizeRanges)
        for (auto countRange : countRanges) {
            double avgRecordSize = (sizeRange.min + sizeRange.max) / 2.0;
            if (avgRecordSize * countRange.min < (10 * GiB)) { // Skip combinations that are very large
                std::cout << typeName << " : " << dataType << " : "
                          << utils::prettySize(sizeRange.min) << " - " << utils::prettySize(sizeRange.max) << " : "
                          << countRange.min << " - " << countRange.max << "\n";

                utils::resetPeakMemUsage();

                StorePtr store = initStore(type, countRange.min, sizeRange, dataGen);

                BenchmarkData insertData{typeName, "insert", sizeRange, countRange, dataType};
                for (int rep = 0; rep < repeats; rep++) {
                    if (store->count() >= countRange.max) { // on small sizes repeat may be more than size range
                        store.reset(); // close the store first (LevelDB has a lock)
                        store = initStore(type, countRange.min, sizeRange, dataGen);
                    }
                    string key = utils::genKey(store->count());
                    string value = dataGen(sizeRange);
                    auto time = utils::timeIt([&]() { store->insert(key, value); });
                    insertData.stats.record(time.count());
                }

                BenchmarkData getData{typeName, "get", sizeRange, countRange, dataType};
                for (int rep = 0; rep < repeats; rep++) {
                    string key = pickKey(store);
                    string value;
                    auto time = utils::timeIt([&]() { value = store->get(key); });
                    getData.stats.record(time.count());
                }

                BenchmarkData updateData{typeName, "update", sizeRange, countRange, dataType};
                for (int rep = 0; rep < repeats; rep++) {
                    string key = pickKey(store);
                    string value = dataGen(sizeRange);
                    auto time = utils::timeIt([&]() { store->update(key, value); });
                    updateData.stats.record(time.count());
                }

                BenchmarkData removeData{typeName, "remove", sizeRange, countRange, dataType};
                for (int rep = 0; rep < repeats; rep++) {
                    string key = pickKey(store);
                    auto time = utils::timeIt([&]() { store->remove(key); });
                    removeData.stats.record(time.count());

                    // Put the key back so we don't have to worry about if a key from genKey is still in the Store
                    string value = dataGen(sizeRange);
                    store->insert(key, value);
                }

                size_t peakMem = std::max((signed long long) (utils::getPeakMemUsage() - baseMemUsage), 0LL);
                BenchmarkData memoryData{typeName, "memory", sizeRange, countRange, dataType};
                memoryData.stats.record(peakMem);

                path filepath = store->filepath;
                size_t dataSize = getDataSize(store);
                store.reset(); // close the store

                size_t diskSize = utils::diskUsage(filepath);
                int spaceEfficiencyPercent = std::round(((double) dataSize / diskSize) * 100);

                fs::remove_all(filepath); // Delete the store files

                BenchmarkData spaceData{typeName, "space", sizeRange, countRange, dataType};
                spaceData.stats.record(spaceEfficiencyPercent); // store as percent

                for (auto& data : {insertData, updateData, getData, removeData, memoryData, spaceData}) {
                    output << toCSVRow(data);
                }
            }
        }
    }
};

int main(int argc, char** argv) {
    doctest::Context context;
    context.setOption("minimal", true); // only show if errors occur.
    context.applyCommandLine(argc, argv);
    int res = context.run();
    if(context.shouldExit()) return res;

    std::cout << "Starting benchmark...\n";

    const std::time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    std::stringstream nowStr;
    nowStr << std::put_time(std::localtime(&now), "%Y%m%d%H%M%S");
    path outFilePath = path("out") / "benchmarks" / ("benchmark"s + nowStr.str() + ".csv");

    fs::create_directories(outFilePath.parent_path());
    std::ofstream output(outFilePath);

    utils::ClobGenerator randClob{"./randomText"};
    Benchmark benchmark{
        "out/stores", // storeDir
        100, // repeats
        { // sizeRanges
            {1, 1*KiB - 1},
            {1*KiB, 10*KiB - 1},
            {10*KiB, 100*KiB - 1},
            {100*KiB, 1*MiB - 1},
        }, { // countRanges
            {100, 1'000 - 1},
            {10'000, 100'000 - 1},
            {1'000'000, 10'000'000 - 1},
        }, { // dataTypes
            {"incompressible", [](auto size) { return utils::randBlob(size); }},
            {"compressible", randClob},
        },
    };
    benchmark.run(output);

    std::cout << "Benchmark written to " << std::quoted(outFilePath.native()) << "\n";
}