#include <iostream>
#include <filesystem>
#include <memory>

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include "dbwrappers.h"

namespace tests {
    using std::unique_ptr, std::make_unique, std::vector, std::array;
    using namespace std::string_literals;

    TEST_CASE("Test DBWrappers") {
        std::filesystem::remove_all("dbs/tests");
        std::filesystem::create_directories("dbs/tests/");

        vector<unique_ptr<dbwrappers::DBWrapper>> dbs{}; // can't use initializer list with unique_ptr for some reason
        dbs.push_back(make_unique<dbwrappers::SQLiteWrapper>("dbs/tests/sqlite3.db"));
        dbs.push_back(make_unique<dbwrappers::LevelDBWrapper>("dbs/tests/leveldb.db"));
        dbs.push_back(make_unique<dbwrappers::RocksDBWrapper>("dbs/tests/rocksdb.db"));
        dbs.push_back(make_unique<dbwrappers::BerkeleyDBWrapper>("dbs/tests/berkleydb.db"));

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
}