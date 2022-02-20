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

#include <boost/json/src.hpp>
#include <boost/uuid/detail/sha1.hpp>

#include "stores.h"
#include "utils.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "tests.cpp"

namespace json = boost::json;
namespace chrono = std::chrono;
namespace filesystem = std::filesystem;
using namespace std::string_literals;
using boost::uuids::detail::sha1;
using std::vector, std::map, std::tuple, std::pair, std::string, filesystem::path, std::size_t;
using utils::Range, utils::Stats, stores::Store, utils::KiB, utils::MiB, utils::GiB;
using StorePtr = std::unique_ptr<stores::Store>;

struct BenchmarkRecord {
    string store;
    string op;
    Range<size_t> size;
    Range<size_t> records;
    Stats<long long> stats{};
};

/** Custom boost JSON conversion. */
void tag_invoke(const json::value_from_tag&, json::value& jv, BenchmarkRecord const& r) {
    jv = {
        {"store", r.store},
        {"op", r.op},
        {"size", utils::prettySize(r.size.min) + " - " + utils::prettySize(r.size.max + 1)},
        {"records", std::to_string(r.records.min) + " - " + std::to_string(r.records.max)},
        {"measurements", r.stats.count()},
        {"sum", r.stats.sum()},
        {"min", r.stats.min()},
        {"max", r.stats.max()},
        {"avg", r.stats.avg()},
    };
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

/**
 * Generate a random key by hashing a number. This allows us to easily get a random key from the store without having
 * to save all the keys we've added.
 */
std::string genKey(size_t i) {
    sha1 hash;
    hash.process_bytes(reinterpret_cast<void*>(&i), sizeof(i));
    hash.process_byte(136); // A salt
    sha1::digest_type digest;
    hash.get_digest(digest);

    std::stringstream ss;
    for (auto part : digest)
        ss << std::setfill('0') << std::setw(sizeof(part) * 2) << std::hex << part;
    return ss.str().substr(0, 32);
}

/** Picks a random key from the store */
std::string pickKey(StorePtr& store) {
   return genKey(utils::randInt<size_t>(0, store->count() - 1));
}

std::string randValue(Range<size_t> size) {
    return utils::randBlob(utils::randInt(size.min, size.max));
}

path getStorePath(stores::Type type) {
    return path("out") / "stores" / stores::types.at(type);
}

StorePtr initStore(stores::Type type, size_t recordCount, Range<size_t> sizeRange) {
    StorePtr store = getStore(type, getStorePath(type));
    for (size_t i = 0; i < recordCount; i++) {
        store->insert(genKey(store->count()), randValue(sizeRange));
    }
    return store;
}

vector<BenchmarkRecord> runBenchmark() {
    filesystem::remove_all("out/stores");
    filesystem::create_directories("out/stores");

    vector<BenchmarkRecord> records;

    for (auto sizeRange : sizeRanges)
    for (auto countRange : countRanges)
    for (auto [type, typeName] : stores::types) {
        StorePtr store = initStore(type, countRange.min, sizeRange);

        BenchmarkRecord insertData{typeName, "insert", sizeRange, countRange};
        for (int rep = 0; rep < REPEATS; rep++) {
            if (store->count() >= countRange.max) { // on small sizes repeat may be more than size range
                store.reset(); // close the store first (LevelDB has a lock)
                store = initStore(type, countRange.min, sizeRange);
            }
            string key = genKey(store->count());
            string value = randValue(sizeRange);
            auto time = utils::timeIt([&]() { store->insert(key, value); });
            insertData.stats.record(time.count());
        }

        BenchmarkRecord getData{typeName, "get", sizeRange, countRange};
        for (int rep = 0; rep < REPEATS; rep++) {
            string key = pickKey(store);
            string value;
            auto time = utils::timeIt([&]() { value = store->get(key); });
            getData.stats.record(time.count());
        }

        BenchmarkRecord updateData{typeName, "update", sizeRange, countRange};
        for (int rep = 0; rep < REPEATS; rep++) {
            string key = pickKey(store);
            string value = randValue(sizeRange);
            auto time = utils::timeIt([&]() { store->update(key, value); });
            updateData.stats.record(time.count());
        }

        BenchmarkRecord removeData{typeName, "remove", sizeRange, countRange};
        for (int rep = 0; rep < REPEATS; rep++) {
            string key = pickKey(store);
            auto time = utils::timeIt([&]() { store->remove(key); });
            removeData.stats.record(time.count());

            // Put the key back so we don't have to worry about if a key from genKey is still in the Store
            string value = randValue(sizeRange);
            store->insert(key, value);
        }

        records.insert(records.end(), {insertData, updateData, getData, removeData});
    }

    return records;
}


int main(int argc, char** argv) {
    doctest::Context context;
    context.setOption("minimal", true); // only show if errors occur.
    context.applyCommandLine(argc, argv);
    int res = context.run();
    if(context.shouldExit()) return res;

    vector<BenchmarkRecord> records = runBenchmark();

    const std::time_t now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    std::stringstream nowStr;
    nowStr << std::put_time(std::localtime(&now), "%Y%m%d%H%M%S");

    path outFileName = path("out") / ("benchmark-"s + nowStr.str() + ".json");
    std::ofstream output;
    output.open(outFileName);

    output << "[\n";
    for (size_t i = 0; i < records.size(); i++) {
        output << "    " << json::value_from(records[i]) << (i < records.size() - 1 ? "," : "") << "\n";
    }
    output << "]\n";

    output.close();

    std::cout << "Benchmark written to " << std::quoted(outFileName.native()) << "\n";
}