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
using std::string, std::to_string, std::vector, std::pair, std::function, std::make_unique;
using utils::Range, stores::Store, utils::KiB, utils::MiB, utils::GiB;
using StorePtr = std::unique_ptr<stores::Store>;
using Stats = utils::Stats<long long>;

struct UsagePattern {
    /** Size range in bytes */
    Range<size_t> size;
    /** Record count range */
    Range<size_t> count;
    /** One of "compressible", "incompressible" */
    string dataType;
};

/** A callable that generates random data for use as a value in the store */
using DataGenerator = function<string(Range<size_t>)>;
/** A callable that creates a new store from (storeType, filepath, pattern). */
using StoreFactory = function<StorePtr(string, path, const UsagePattern&)>;

/** This class runs the actual benchmark */
class Benchmark {
public:
    /** Directory to save the stores */
    path storeDir;

    /** Name of the system the benchmark is running on */
    string hardware;

    /** How many iterations of each measurement to do */
    const int repeats;

    /**
     * Combinations of size and count that would lead to greater than this DB size will be skipped
     * Does not take compression or spaceEfficiency into account which predicting size
     */
    const size_t maxDbSize;

    /** The names of the stores to compare */
    const vector<string> storeTypes;

    /** A callable that creates a new store from (storeType, filepath, pattern). */
    const StoreFactory storeFactory;

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

    StorePtr initStore(string storeType, const UsagePattern& pattern, DataGenerator dataGen) {
        StorePtr store = storeFactory(storeType, storeDir / storeType, pattern);
        int batchSize = 500;
        for (size_t i = 0; i < pattern.count.min; i += batchSize) { // insert in 100 item batches
            vector<pair<string, string>> batch;
            for (size_t j = i; j < std::min(pattern.count.min, i + batchSize); j++) {
                batch.push_back({utils::genKey(j), dataGen(pattern.size)});
            }
            store->bulkInsert(batch);
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

    inline static const string CSV_HEADER = "hardware,store,op,size,records,data type,measurements,sum,min,max,avg\n";
    string getCSVRow(const string& store, const string& op, const UsagePattern& pattern, const Stats& stats) {
        return hardware + "," + store + "," + op + "," +
            utils::prettySize(pattern.size.min) + " to " + utils::prettySize(pattern.size.max + 1) + "," +
            to_string(pattern.count.min) + "," +
            pattern.dataType + "," +
            to_string(stats.count()) + "," +
            to_string(stats.sum()) + "," +
            to_string(stats.min()) + "," +
            to_string(stats.max()) + "," +
            to_string(stats.avg()) + "\n";
    }

    /** Runs the benchmark. Pass the output stream to save CSV data to */
    void run(std::ostream& output) {
        fs::remove_all(storeDir); // clear the storeDir
        fs::create_directories(storeDir);
        output << CSV_HEADER;

        utils::resetPeakMemUsage();
        size_t baseMemUsage = utils::getPeakMemUsage(); // We'll subtract the base from future measurements

        for (auto storeType : storeTypes)
        for (auto [dataType, dataGen] : dataTypes)
        for (auto sizeRange : sizeRanges)
        for (auto countRange : countRanges) {
            UsagePattern pattern{sizeRange, countRange, dataType};

            size_t avgRecordSize = (sizeRange.min + sizeRange.max) / 2;
            size_t predictedSize = avgRecordSize * std::min(countRange.min + repeats, countRange.max);
            if (predictedSize < maxDbSize) { // Skip combinations that are very large
                std::cout << storeType << ", " << dataType << ", "
                          << utils::prettySize(sizeRange.min) << " to " << utils::prettySize(sizeRange.max + 1) << ", "
                          << countRange.min << " to " << countRange.max << "\n";

                utils::resetPeakMemUsage();

                StorePtr store = initStore(storeType, pattern, dataGen);

                Stats insertStats;
                for (int rep = 0; rep < repeats; rep++) {
                    if (store->count() >= countRange.max) { // on small sizes repeat may be more than size range
                        store.reset(); // close the store first (LevelDB has a lock)
                        store = initStore(storeType, pattern, dataGen);
                    }
                    string key = utils::genKey(store->count());
                    string value = dataGen(sizeRange);
                    auto time = utils::timeIt([&]() { store->insert(key, value); });
                    insertStats.record(time.count());
                }

                Stats getStats;
                for (int rep = 0; rep < repeats; rep++) {
                    string key = pickKey(store);
                    string value;
                    auto time = utils::timeIt([&]() { value = store->get(key); });
                    getStats.record(time.count());
                }

                Stats updateStats;
                for (int rep = 0; rep < repeats; rep++) {
                    string key = pickKey(store);
                    string value = dataGen(sizeRange);
                    auto time = utils::timeIt([&]() { store->update(key, value); });
                    updateStats.record(time.count());
                }

                Stats removeStats;
                for (int rep = 0; rep < repeats; rep++) {
                    string key = pickKey(store);
                    auto time = utils::timeIt([&]() { store->remove(key); });
                    removeStats.record(time.count());

                    // Put the key back so we don't have to worry about if a key from genKey is still in the Store
                    string value = dataGen(sizeRange);
                    store->insert(key, value);
                }

                long long peakMem = std::max((signed long long) (utils::getPeakMemUsage() - baseMemUsage), 0LL);
                Stats memoryStats{peakMem};

                path filepath = store->filepath;
                size_t dataSize = getDataSize(store);
                store.reset(); // close the store

                size_t diskSize = utils::diskUsage(filepath);
                int spaceEfficiencyPercent = std::round(((double) dataSize / diskSize) * 100);
                Stats spaceStats{spaceEfficiencyPercent}; // store as percent

                fs::remove_all(filepath); // Delete the store files

                output << getCSVRow(storeType, "insert", pattern, insertStats);
                output << getCSVRow(storeType, "update", pattern, updateStats);
                output << getCSVRow(storeType, "get", pattern, getStats);
                output << getCSVRow(storeType, "remove", pattern, removeStats);
                output << getCSVRow(storeType, "memory", pattern, memoryStats);
                output << getCSVRow(storeType, "space", pattern, spaceStats);
                output.flush();
            }
        }
    }
};


StorePtr storeFactory(string storeType, path filepath, const UsagePattern& pattern) {
    if (storeType == "SQLite3") {
        return make_unique<stores::SQLite3Store>(filepath);
    } else if (storeType == "LevelDB") {
        leveldb::Options options;
        options.compression = (pattern.dataType == "compressible") ?
            leveldb::CompressionType::kSnappyCompression :
            leveldb::CompressionType::kNoCompression;
        return make_unique<stores::LevelDBStore>(filepath, options); // TODO
    } else if (storeType == "RocksDB") {
        rocksdb::Options options;
        options.compression = (pattern.dataType == "compressible") ?
            rocksdb::CompressionType::kSnappyCompression :
            rocksdb::CompressionType::kNoCompression;
        return make_unique<stores::RocksDBStore>(filepath, options);
    } else if (storeType == "BerkeleyDB") {
        return make_unique<stores::BerkeleyDBStore>(filepath);
    } else if (storeType == "FlatFolder") {
        return make_unique<stores::FlatFolderStore>(filepath);
    } else if (storeType == "NestedFolder") {
        // using 32 char hash (128) so we don't have to worry about collisions
        // 3 levels of nesting with 2 chars and a max of 10,000,000 records should have 2 levels with 265
        // folders and and about 142 files at the lowest level on average.
        return make_unique<stores::NestedFolderStore>(filepath, 2, 3, 32);
    } else {
        throw std::runtime_error("Unknown store type "s + storeType);
    }
}


int main(int argc, char** argv) {
    doctest::Context context;
    context.setOption("minimal", true); // only show if errors occur.
    context.applyCommandLine(argc, argv);
    int res = context.run();
    if(context.shouldExit()) return res;

    string hardware; 
    std::cout << "Name of the system the benchmark is running on: ";
    std::cin >> hardware; // Get user input from the keyboard

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
        hardware, // hardware
        1000, // repeats
        10 * GiB, // maxDbSize
        {"LevelDB", "RocksDB", "BerkeleyDB", "FlatFolder", "NestedFolder", "SQLite3"}, // storeTypes
        storeFactory, // storeFactory
        { // sizeRanges
            {1, 1*KiB - 1},
            {1*KiB, 10*KiB - 1},
            {10*KiB, 100*KiB - 1},
            {100*KiB, 1*MiB - 1},
        }, { // countRanges
            {100, 500},
            {1'000, 5'000},
            {10'000, 50'000},
            {100'000, 150'000},
            {1'000'000, 1'050'000},
        }, { // dataTypes
            {"incompressible", [](auto size) { return utils::randBlob(size); }},
            {"compressible", randClob},
        },
    };
    benchmark.run(output);

    std::cout << "Benchmark written to " << std::quoted(outFilePath.native()) << "\n";
}