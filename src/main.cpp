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

#include "dbwrappers.h"
#include "helpers.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "tests.cpp"

using std::vector, std::map, std::tuple, std::string, std::unique_ptr, std::make_unique;
namespace json = boost::json;
namespace chrono = std::chrono;

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

    /** Note: Divide by zero if we haven't recording anything */
    T count() { return _count; }
    T sum() { return _sum; }
    T min() { return _min; }
    T max() { return _max; }
    T avg() { return _sum / _count; }
};


using BenchmarkData = map<vector<string>, Stats<long long>>;

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
        (*obj)["sum"] = stats.sum();
        (*obj)["min"] = stats.min();
        (*obj)["max"] = stats.max();
        (*obj)["avg"] = stats.avg();
    }

    return root;
}

BenchmarkData runBenchmark() {
    std::filesystem::remove_all("out/dbs");
    std::filesystem::create_directories("out/dbs");

    vector<unique_ptr<dbwrappers::DBWrapper>> dbs{}; // can't use initializer list with unique_ptr for some reason
    dbs.push_back(make_unique<dbwrappers::SQLiteWrapper>("out/dbs/sqlite3.db"));
    dbs.push_back(make_unique<dbwrappers::LevelDBWrapper>("out/dbs/leveldb.db"));
    dbs.push_back(make_unique<dbwrappers::RocksDBWrapper>("out/dbs/rocksdb.db"));
    dbs.push_back(make_unique<dbwrappers::BerkeleyDBWrapper>("out/dbs/berkleydb.db"));

    BenchmarkData data;

    for (auto& db : dbs) {
        for (int i = 0; i < 100; i++) {
            string key = helpers::randHash();
            string blob = helpers::randBlob(1024);

            auto time = helpers::timeIt([&]() { db->insert(key, blob); });
            data[{db->type(), "insert"}].record(time.count());

            blob = helpers::randBlob(1024);
            time = helpers::timeIt([&]() { db->update(key, blob); });
            data[{db->type(), "update"}].record(time.count());

            string value;
            time = helpers::timeIt([&]() {
                value = db->get(key);
            });
            if (value.size() == 0) throw std::runtime_error("DB get failed"); // make sure compiler doe
            data[{db->type(), "get"}].record(time.count());

            time = helpers::timeIt([&]() { db->remove(key); });
            data[{db->type(), "remove"}].record(time.count());
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