#include <iostream>
#include <filesystem>
#include <random>
#include <memory>
#include <chrono>
#include <vector>
#include <map>
#include <tuple>
#include <cmath>

#include "dbwrappers.h"
#include "helpers.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "tests.cpp"

using std::vector, std::map, std::tuple, std::string, std::unique_ptr, std::make_unique;

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

class BenchmarkData {
public:
    map<tuple<string, string>, Stats<long long>> data;
    void record(string db, string operation, long long value) {
        data[{db, operation}].record(value);
    }

    Stats<long long> get(string db, string operation) {
        return data[{db, operation}];
    }
};


BenchmarkData runBenchmark() {
    std::filesystem::remove_all("dbs");
    std::filesystem::create_directories("dbs");

    vector<unique_ptr<dbwrappers::DBWrapper>> dbs{}; // can't use initializer list with unique_ptr for some reason
    dbs.push_back(make_unique<dbwrappers::SQLiteWrapper>("dbs/sqlite3.db"));
    dbs.push_back(make_unique<dbwrappers::LevelDBWrapper>("dbs/leveldb.db"));
    dbs.push_back(make_unique<dbwrappers::RocksDBWrapper>("dbs/rocksdb.db"));
    dbs.push_back(make_unique<dbwrappers::BerkeleyDBWrapper>("dbs/berkleydb.db"));

    BenchmarkData data;

    for (auto& db : dbs) {
        for (int i = 0; i < 100; i++) {
            string key = helpers::randHash();
            string blob = helpers::randBlob(1024);

            auto time = helpers::timeIt([&]() { db->insert(key, blob); });
            data.record(db->type(), "insert", time.count());

            blob = helpers::randBlob(1024);
            time = helpers::timeIt([&]() { db->update(key, blob); });
            data.record(db->type(), "update", time.count());

            string value;
            time = helpers::timeIt([&]() {
                value = db->get(key);
            });
            if (value.size() == 0) throw std::runtime_error("DB get failed"); // make sure compiler doe
            data.record(db->type(), "get", time.count());

            time = helpers::timeIt([&]() { db->remove(key); });
            data.record(db->type(), "remove", time.count());
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

    for (auto& record : data.data) {
        std::cout << "[";
        std::apply([](auto&&... t) { ((std::cout << t << ", "), ...); }, record.first);
        std::cout << "] : " << record.second.avg() << "ns" << std::endl;
    }
}