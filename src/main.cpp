#include <iostream>
#include <filesystem>
#include <random>
#include <memory>

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include "dbwrappers.cpp"

using std::unique_ptr, std::make_unique, std::exception, std::runtime_error,
      std::vector, std::array, std::string, std::size_t;
using namespace std::string_literals;

// std::random_device rd;
// std::mt19937 randGen(rd());

// string randBlob(size_t size) {
//     std::uniform_int_distribution<unsigned char> randChar(0, 10);
//     string blob(size, '\0');
//     for (size_t i = 0; i < size; i++) {
//         blob[i] = randChar(randGen);
//     }
//     return blob;
// }


TEST_CASE("Test DBWrappers") {
    std::filesystem::remove_all("dbs/tests");
    std::filesystem::create_directories("dbs/tests/");

    array<unique_ptr<dbwrappers::DBWrapper>, 1> dbs{
        make_unique<dbwrappers::SQLiteWrapper>("dbs/tests/sqlite3.db"),
        // make_unique<dbwrappers::LevelDBWrapper>("dbs/tests/leveldb.db"),
        // make_unique<dbwrappers::RocksDBWrapper>("dbs/tests/rocksdb.db"),
        // make_unique<dbwrappers::BerkeleyDBWraper>("dbs/tests/berkleydb.db"),
    };

    SUBCASE("Basic") {
        for (auto& db : dbs) {
            INFO(db->type());

            db->insert("key", "value");
            REQUIRE(db->get("key") == "value");

            db->update("key", "updated");
            REQUIRE(db->get("key") == "updated");

            db->remove("key");

            REQUIRE_THROWS(db->get("key"));
        }
    }

    SUBCASE("Nulls") {
        for (auto& db : dbs) {
            INFO(db->type());

            db->insert("key", "hello\0world"s);
            REQUIRE(db->get("key") == "hello\0world"s);

            db->update("key", "\0goodbye\0"s);
            REQUIRE(db->get("key") == "\0goodbye\0"s);
        }
    }
}

int main(int argc, char** argv) {
    doctest::Context context;
    context.setOption("minimal", true); // only show if errors occur.
    context.applyCommandLine(argc, argv);
    int res = context.run();
    if(context.shouldExit()) return res;
}