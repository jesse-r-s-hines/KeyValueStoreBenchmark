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

#include <boost/json/src.hpp>

#include "stores.h"
#include "utils.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "tests.cpp"

namespace json = boost::json;
namespace chrono = std::chrono;
namespace filesystem = std::filesystem;
using namespace std::string_literals;
using std::vector, std::map, std::tuple, std::pair, std::string, filesystem::path, std::size_t;
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

/** Custom boost JSON conversion. */
void tag_invoke(const json::value_from_tag&, json::value& jv, BenchmarkData const& r) {
    jv = {
        {"store", r.store},
        {"op", r.op},
        {"size", utils::prettySize(r.size.min) + "-" + utils::prettySize(r.size.max + 1)},
        {"records", std::to_string(r.records.min) + "-" + std::to_string(r.records.max)},
        {"dataType", r.dataType},
        {"measurements", r.stats.count()},
        {"sum", r.stats.sum()},
        {"min", r.stats.min()},
        {"max", r.stats.max()},
        {"avg", r.stats.avg()},
    };
}

string benchmarkDataToJSON(vector<BenchmarkData> data) {
    std::stringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < data.size() - 1; i++)
        ss << "    " << json::value_from(data[i]) << ",\n";
    if (data.size() > 0)
        ss << "    " << json::value_from(data.back()) << "\n";
    ss << "]\n";
    return ss.str();
}

string benchmarkDataToCSV(vector<BenchmarkData> data) {
    std::stringstream ss;
    ss << "store,op,size,records,data type,measurements,sum,min,max,avg\n";

    for (auto& r : data) {
        ss << r.store << ","
           << r.op << ","
           << utils::prettySize(r.size.min) + " to " + utils::prettySize(r.size.max + 1) << ","
           << std::to_string(r.records.min) + " to " + std::to_string(r.records.max) << ","
           << r.dataType << ","
           << r.stats.count() << ","
           << r.stats.sum() << ","
           << r.stats.min() << ","
           << r.stats.max() << ","
           << r.stats.avg() << "\n";
    }

    return ss.str();
}


/** How many iterations of each measurement to do */
const int REPEATS = 100;

/** Size ranges to test [min, max] */
const vector<Range<size_t>> sizeRanges{
    {1, 1*KiB - 1},
    {1*KiB, 10*KiB - 1},
    // {10*KiB, 100*KiB - 1},
    // {100*KiB, 1*MiB - 1},
    // {1*MiB, 10*MiB - 1},
};

/** Record count ranges to test [min, max] */
const vector<Range<size_t>> countRanges{
    {1, 10 - 1},
    {10, 100 - 1},
    // {100, 1'000 - 1},
    // {1'000, 10'000 - 1},
    // {10'000, 100'000 - 1},
    // {100'000, 1'000'000 - 1},
    // {1'000'000, 10'000'000 - 1},
};

using DataGenerator = string (*)(Range<size_t>);

/** Incompressible vs compressible data */
const vector<pair<string, DataGenerator>> dataTypes{
    {"incompressible", utils::randBlob},
    {"compressible", utils::randClob},
};


/** Picks a random key from the store */
std::string pickKey(StorePtr& store) {
   return utils::genKey(utils::randInt<size_t>(0, store->count() - 1));
}

path getStorePath(stores::Type type) {
    return path("out") / "stores" / stores::types.at(type);
}

StorePtr initStore(stores::Type type, size_t recordCount, Range<size_t> sizeRange, DataGenerator dataGen) {
    StorePtr store = getStore(type, getStorePath(type));
    for (size_t i = 0; i < recordCount; i++) {
        store->insert(utils::genKey(store->count()), dataGen(sizeRange));
    }
    return store;
}


vector<BenchmarkData> runBenchmark() {
    filesystem::remove_all("out/stores");
    filesystem::create_directories("out/stores");

    vector<BenchmarkData> records;

    for (auto sizeRange : sizeRanges)
    for (auto countRange : countRanges)
    for (auto [dataType, dataGen] : dataTypes)
    for (auto [type, typeName] : stores::types) {
        StorePtr store = initStore(type, countRange.min, sizeRange, dataGen);

        BenchmarkData insertData{typeName, "insert", sizeRange, countRange, dataType};
        for (int rep = 0; rep < REPEATS; rep++) {
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

        path filepath = store->filepath;
        // Maybe we could keep a count of the exact size? (But updates would complicate that...)
        size_t dataSize = store->count() * ((sizeRange.min + sizeRange.max) / 2.0);
        store.reset(); // close the store

        size_t diskSize = utils::diskUsage(filepath);
        int spaceEfficiencyPercent = std::round(((double) dataSize / diskSize) * 100);

        BenchmarkData spaceData{typeName, "space", sizeRange, countRange, dataType};
        spaceData.stats.record(spaceEfficiencyPercent); // store as percent

        records.insert(records.end(), {insertData, updateData, getData, removeData, spaceData});
    }

    return records;
}


int main(int argc, char** argv) {
    filesystem::current_path(filesystem::absolute(argv[0]).parent_path().c_str());

    doctest::Context context;
    context.setOption("minimal", true); // only show if errors occur.
    context.applyCommandLine(argc, argv);
    int res = context.run();
    if(context.shouldExit()) return res;

    std::cout << "Starting benchmark...\n";

    vector<BenchmarkData> data = runBenchmark();

    const std::time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    std::stringstream nowStr;
    nowStr << std::put_time(std::localtime(&now), "%Y%m%d%H%M%S");

    string format = "csv"; // or json
    string outFileName;
    string strRepr;
    if (format == "csv") {
        outFileName = "benchmark"s + nowStr.str() + ".csv";
        strRepr = benchmarkDataToCSV(data);
    } else {
        outFileName = "benchmark"s + nowStr.str() + ".json";
        strRepr = benchmarkDataToJSON(data);
    }

    std::ofstream output;
    path outFilePath = path("out") / "benchmarks" / outFileName;
    filesystem::create_directories(outFilePath.parent_path());
    output.open(outFilePath);
    output << strRepr;
    
    std::cout << "Benchmark written to " << std::quoted(outFilePath.native()) << "\n";
}