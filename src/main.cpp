#include <iostream>
#include <fstream>
#include <filesystem>
#include <memory>
#include <chrono>
#include <vector>
#include <map>
#include <tuple>

#include <boost/json/src.hpp>

#include "stores.h"
#include "utils.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "tests.cpp"

using std::vector, std::map, std::tuple, std::pair, std::string, std::unique_ptr, std::make_unique;
using namespace std::string_literals;
namespace json = boost::json;
namespace chrono = std::chrono;

struct BenchmarkRecord {
    string store;
    string op;
    helpers::Stats<long long> stats{};
};

/** Custom boost JSON conversion. */
void tag_invoke(const json::value_from_tag&, json::value& jv, BenchmarkRecord const& r) {
    jv = {
        {"store", r.store        },
        {"op"   , r.op           },
        {"count", r.stats.count()},
        {"sum"  , r.stats.sum()  },
        {"min"  , r.stats.min()  },
        {"max"  , r.stats.max()  },
        {"avg"  , r.stats.avg()  },
    };
}

vector<BenchmarkRecord> runBenchmark() {
    std::filesystem::remove_all("out/dbs");
    std::filesystem::create_directories("out/dbs");

    vector<unique_ptr<stores::Store>> dbs{}; // can't use initializer list with unique_ptr for some reason
    dbs.push_back(make_unique<stores::SQLiteStore>("out/dbs/sqlite3.db"));
    dbs.push_back(make_unique<stores::LevelDBStore>("out/dbs/leveldb.db"));
    dbs.push_back(make_unique<stores::RocksDBStore>("out/dbs/rocksdb.db"));
    dbs.push_back(make_unique<stores::BerkeleyDBStore>("out/dbs/berkeleydb.db"));

    vector<BenchmarkRecord> records;

    for (auto& db : dbs) {
        BenchmarkRecord insertData{db->type(), "insert"};
        BenchmarkRecord updateData{db->type(), "update"};
        BenchmarkRecord getData   {db->type(), "get"   };
        BenchmarkRecord removeData{db->type(), "remove"};

        for (int i = 0; i < 100; i++) {
            string key = helpers::randHash();
            string blob = helpers::randBlob(1024);

            auto time = helpers::timeIt([&]() { db->insert(key, blob); });
            insertData.stats.record(time.count());

            blob = helpers::randBlob(1024);
            time = helpers::timeIt([&]() { db->update(key, blob); });
            updateData.stats.record(time.count());

            string value;
            time = helpers::timeIt([&]() {
                value = db->get(key);
            });
            if (value.size() == 0) throw std::runtime_error("DB get failed"); // make sure compiler doe
            getData.stats.record(time.count());

            time = helpers::timeIt([&]() { db->remove(key); });
            removeData.stats.record(time.count());
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

    std::ofstream output;
    output.open("out/benchmark.json");
    output << json::value_from(records);
    output.close();
}