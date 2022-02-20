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
using std::vector, std::map, std::tuple, std::pair, std::string, std::unique_ptr, std::make_unique, filesystem::path;

struct BenchmarkRecord {
    string store;
    string op;
    utils::Stats<long long> stats{};
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

path getStorePath(string name) {
    return path("out") / "stores" / name;
};

vector<BenchmarkRecord> runBenchmark() {
    filesystem::remove_all("out/stores");
    filesystem::create_directories("out/stores");

    vector<unique_ptr<stores::Store>> stores{};
    for (auto [type, typeName] : stores::types)
        stores.push_back(stores::getStore(type, getStorePath(typeName)));

    vector<BenchmarkRecord> records;

    for (auto& store : stores) {
        BenchmarkRecord insertData{store->typeName(), "insert"};
        BenchmarkRecord updateData{store->typeName(), "update"};
        BenchmarkRecord getData   {store->typeName(), "get"   };
        BenchmarkRecord removeData{store->typeName(), "remove"};

        for (int i = 0; i < 100; i++) {
            string key = utils::randHash(32);
            string blob = utils::randBlob(1024);

            auto time = utils::timeIt([&]() { store->insert(key, blob); });
            insertData.stats.record(time.count());

            blob = utils::randBlob(1024);
            time = utils::timeIt([&]() { store->update(key, blob); });
            updateData.stats.record(time.count());

            string value;
            time = utils::timeIt([&]() {
                value = store->get(key);
            });
            if (value.size() == 0) throw std::runtime_error("DB get failed"); // make sure compiler doe
            getData.stats.record(time.count());

            time = utils::timeIt([&]() { store->remove(key); });
            removeData.stats.record(time.count());
        }

        records.insert(records.end(), {insertData, updateData, getData, removeData});
    }

    vector<pair<BenchmarkRecord, string>> sizeRecords;
    for (auto& store : stores)
        sizeRecords.push_back({{store->typeName(), "size"}, store->filepath});
    stores.clear(); // delete and close all stores so we can get final size

    for (auto& sizeRecord : sizeRecords) {
        sizeRecord.first.stats.record(utils::diskUsage(sizeRecord.second));
        records.push_back(sizeRecord.first);
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