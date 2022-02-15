#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>
#include <memory>
#include <chrono>
#include <vector>
#include <map>
#include <tuple>
#include <cmath>

#include <boost/json/src.hpp>

#include "stores.h"
#include "helpers.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "tests.cpp"

using std::vector, std::map, std::tuple, std::string, std::unique_ptr, std::make_unique;
namespace json = boost::json;
namespace chrono = std::chrono;

/** Keeps the average and other statistics. */
template<typename T>
class Stats {
    long long _count = 0;
    T _sum;
    T _min;
    T _max;

public:
    void record(T value) {
        _sum += value;
        if (_count == 0 || value < _min) _min = value;
        if (_count == 0 || value > _max) _max = value;
        _count++;
    }

    long long count() { return _count; }
    T sum() { return _sum; }
    T min() { return _min; }
    T max() { return _max; }
    /** Note: Throws divide by zero if you haven't recording anything */
    T avg() { return _sum / _count; }
};

/** Maps a vector of matrix options to the gathered statistics. */
using BenchmarkData = map<vector<string>, Stats<chrono::nanoseconds>>;

/**
 * Converts a BenchmarkData into a nested JSON object that will look something like this:
 * ```json
 * {
 *   "berkeleydb": {
 *     "1B-1KiB": {
 *       "insert": {"count": 100, "sum": 540781, "min": 4085, "max": 83592, "avg": 5407},
 *       "update": {"count": 100, "sum": 541223, "min": 4628, "max": 18565, "avg": 5412},
 *       "get": {"count": 100, "sum": 248928, "min": 2192, "max": 12442, "avg": 2489},
 *       "remove": {"count": 100, "sum": 391105, "min": 3631, "max": 12406, "avg": 3911},
 *     },
 *     "1Kib-10Kib": {
 *       ...
 *     },
 *     ...
 *   },
 *   "leveldb": {
 *        ...
 *   },
 *   ...
 * }
 * 
 * ```
 */
json::object benchmarkToJson(BenchmarkData data) {
    json::object root;

    for (auto& [path, stats] : data) {
        json::object* obj = &root;
        for (auto& key : path) {
            if (!obj->contains(key)) {
                (*obj)[key] = json::object();
            }
            obj = &((*obj)[key].as_object());
        }
        (*obj)["count"] = stats.count();
        (*obj)["sum"] = stats.sum().count();  // convert nanoseconds to raw int
        (*obj)["min"] = stats.min().count();
        (*obj)["max"] = stats.max().count();
        (*obj)["avg"] = stats.avg().count();
    }

    return root;
}

BenchmarkData runBenchmark() {
    std::filesystem::remove_all("out/dbs");
    std::filesystem::create_directories("out/dbs");

    vector<unique_ptr<stores::Store>> dbs{}; // can't use initializer list with unique_ptr for some reason
    dbs.push_back(make_unique<stores::SQLiteStore>("out/dbs/sqlite3.db"));
    dbs.push_back(make_unique<stores::LevelDBStore>("out/dbs/leveldb.db"));
    dbs.push_back(make_unique<stores::RocksDBStore>("out/dbs/rocksdb.db"));
    dbs.push_back(make_unique<stores::BerkeleyDBStore>("out/dbs/berkleydb.db"));

    BenchmarkData data;

    for (auto& db : dbs) {
        for (int i = 0; i < 100; i++) {
            string key = helpers::randHash();
            string blob = helpers::randBlob(1024);

            auto time = helpers::timeIt([&]() { db->insert(key, blob); });
            data[{db->type(), "insert"}].record(time);

            blob = helpers::randBlob(1024);
            time = helpers::timeIt([&]() { db->update(key, blob); });
            data[{db->type(), "update"}].record(time);

            string value;
            time = helpers::timeIt([&]() {
                value = db->get(key);
            });
            if (value.size() == 0) throw std::runtime_error("DB get failed"); // make sure compiler doe
            data[{db->type(), "get"}].record(time);

            time = helpers::timeIt([&]() { db->remove(key); });
            data[{db->type(), "remove"}].record(time);
        }
    }

    return data;
}


int main(int argc, char** argv) {
    doctest::Context context;
    context.setOption("minimal", true); // only show if errors occur.
    context.applyCommandLine(argc, argv);
    int res = context.run();
    if(context.shouldExit()) return res;

    BenchmarkData data = runBenchmark();

    std::ofstream output;
    output.open("out/benchmark.json");
    output << benchmarkToJson(data);
    output.close();
}