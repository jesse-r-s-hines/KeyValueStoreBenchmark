#include <iostream>
#include <filesystem>
#include <memory>

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include "stores.h"

namespace tests {
    using std::unique_ptr, std::make_unique, std::vector, std::array;
    using namespace std::string_literals;

    TEST_CASE("Test Stores") {
        std::filesystem::remove_all("out/tests");
        std::filesystem::create_directories("out/tests/");

        vector<unique_ptr<stores::Store>> dbs{}; // can't use initializer list with unique_ptr for some reason
        dbs.push_back(stores::getStore(stores::Type::SQLite3, "out/tests/sqlite3.db", false));
        dbs.push_back(stores::getStore(stores::Type::LevelDB, "out/tests/leveldb.db", false));
        dbs.push_back(stores::getStore(stores::Type::RocksDB, "out/tests/rocksdb.db", false));
        dbs.push_back(stores::getStore(stores::Type::BerkeleyDB, "out/tests/berkeleydb.db", false));

        SUBCASE("Basic") {
            for (auto& db : dbs) {
                INFO(db->typeName());

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
                INFO(db->typeName());

                db->insert("key", "hello\0world"s);
                REQUIRE(db->get("key") == "hello\0world"s);

                db->update("key", "\0goodbye\0"s);
                REQUIRE(db->get("key") == "\0goodbye\0"s);
            }
        }
    }
}