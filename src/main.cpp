#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <chrono>
#include <vector>
#include <map>
#include <tuple>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <functional>

#include "stores.h"
#include "utils.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "tests.cpp"

namespace chrono = std::chrono;
namespace filesystem = std::filesystem;
using namespace std::string_literals;
using std::vector, std::map, std::tuple, std::pair, std::string, filesystem::path, std::size_t, std::to_string;
using std::function;
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

    inline static const string CSV_HEADER = "store,op,size,records,data type,measurements,sum,min,max,avg\n";
    string toCSVRow() const {
        return this->store + "," +
            this->op + "," +
            utils::prettySize(this->size.min) + " to " + utils::prettySize(this->size.max + 1) + "," +
            std::to_string(this->records.min) + " to " + std::to_string(this->records.max) + "," +
            this->dataType + "," +
            to_string(this->stats.count()) + "," +
            to_string(this->stats.sum()) + "," +
            to_string(this->stats.min()) + "," +
            to_string(this->stats.max()) + "," +
            to_string(this->stats.avg()) + "\n";
    }
};

/** How many iterations of each measurement to do */
const int REPEATS = 100;

/** Size ranges to test [min, max] */
const vector<Range<size_t>> sizeRanges{
    {1, 1*KiB - 1},
    {1*KiB, 10*KiB - 1},
    // {10*KiB, 100*KiB - 1},
    // {100*KiB, 1*MiB - 1},
};

/** Record count ranges to test [min, max] */
const vector<Range<size_t>> countRanges{
    {100, 1'000 - 1},
    // {10'000, 100'000 - 1},
    // {1'000'000, 10'000'000 - 1},
};

using DataGenerator = function<string(Range<size_t>)>;
utils::ClobGenerator randClob("./randomText");
/** Incompressible vs compressible data */
const vector<pair<string, DataGenerator>> dataTypes{
    {"incompressible", [](auto size) { return utils::randBlob(size); }},
    {"compressible", randClob},
};


/** Picks a random key from the store */
std::string pickKey(StorePtr& store) {
   return utils::genKey(utils::randInt<size_t>(0, store->count() - 1));
}

StorePtr initStore(path storeDir, stores::Type type, size_t recordCount, Range<size_t> sizeRange, DataGenerator dataGen) {
    StorePtr store = getStore(type, storeDir / stores::types.at(type));
    for (size_t i = 0; i < recordCount; i++) {
        store->insert(utils::genKey(store->count()), dataGen(sizeRange));
    }
    return store;
}

/** Runs the benchmark. Pass directory to put stores in and file to save CSV data to */
void runBenchmark(path storeDir, std::ostream& output) {
    filesystem::remove_all(storeDir); // clear the storeDir
    filesystem::create_directories(storeDir);
    output << BenchmarkData::CSV_HEADER;

    utils::resetPeakMemUsage();
    size_t baseMemUsage = utils::getPeakMemUsage(); // We'll subtract the base from future measurements

    for (auto sizeRange : sizeRanges)
    for (auto countRange : countRanges)
    for (auto [dataType, dataGen] : dataTypes)
    for (auto [type, typeName] : stores::types) {
        double avgSize = (sizeRange.min + sizeRange.max) / 2.0;
        if (avgSize * countRange.max < (10 * GiB)) { // Skip combinations that are very large
            utils::resetPeakMemUsage();

            StorePtr store = initStore(storeDir, type, countRange.min, sizeRange, dataGen);

            BenchmarkData insertData{typeName, "insert", sizeRange, countRange, dataType};
            for (int rep = 0; rep < REPEATS; rep++) {
                if (store->count() >= countRange.max) { // on small sizes repeat may be more than size range
                    store.reset(); // close the store first (LevelDB has a lock)
                    store = initStore(storeDir, type, countRange.min, sizeRange, dataGen);
                }
                string key = utils::genKey(store->count());
                string value = dataGen(sizeRange);
                auto time = utils::timeIt([&]() { store->insert(key, value); });
                insertData.stats.record(time.count());
            }

            BenchmarkData getData{typeName, "get", sizeRange, countRange, dataType};
            for (int rep = 0; rep < REPEATS; rep++) {
                string key = pickKey(store);
                string value;
                auto time = utils::timeIt([&]() { value = store->get(key); });
                getData.stats.record(time.count());
            }

            BenchmarkData updateData{typeName, "update", sizeRange, countRange, dataType};
            for (int rep = 0; rep < REPEATS; rep++) {
                string key = pickKey(store);
                string value = dataGen(sizeRange);
                auto time = utils::timeIt([&]() { store->update(key, value); });
                updateData.stats.record(time.count());
            }

            BenchmarkData removeData{typeName, "remove", sizeRange, countRange, dataType};
            for (int rep = 0; rep < REPEATS; rep++) {
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
            // Maybe we could keep a count of the exact size? (But updates would complicate that...)
            size_t dataSize = store->count() * avgSize;
            store.reset(); // close the store

            size_t diskSize = utils::diskUsage(filepath);
            int spaceEfficiencyPercent = std::round(((double) dataSize / diskSize) * 100);

            BenchmarkData spaceData{typeName, "space", sizeRange, countRange, dataType};
            spaceData.stats.record(spaceEfficiencyPercent); // store as percent

            for (auto& data : {insertData, updateData, getData, removeData, memoryData, spaceData}) {
                output << data.toCSVRow();
            }
        }
    }
}


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

    filesystem::create_directories(outFilePath.parent_path());
    std::ofstream output(outFilePath);

    runBenchmark("out/stores", output);

    std::cout << "Benchmark written to " << std::quoted(outFilePath.native()) << "\n";
}