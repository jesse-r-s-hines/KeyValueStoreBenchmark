/**
 * Some basic tests to make each store type is working properly.
 */
#include <filesystem>
#include <memory>
#include <map>
#include <string>
#include <functional>

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include "stores.h"
#include "utils.h"

namespace tests {
    namespace fs = std::filesystem;
    using fs::path;
    using namespace std::string_literals;
    using std::string, std::vector, std::map, std::function, std::unique_ptr, std::make_unique;
    using stores::Store;

    const string filepath = path("out") / "tests" / "store";

    vector<function<unique_ptr<Store>()>> storeFactories{
        [](){ return make_unique<stores::SQLite3Store>(filepath); },
        [](){ return make_unique<stores::LevelDBStore>(filepath); },
        [](){ return make_unique<stores::RocksDBStore>(filepath); },
        [](){ return make_unique<stores::BerkeleyDBStore>(filepath); },
        [](){ return make_unique<stores::FlatFolderStore>(filepath); },
        [](){ return make_unique<stores::NestedFolderStore>(filepath, 2, 3, 32); },
    };


    TEST_CASE("Test Stores") {
        fs::remove_all("out/tests");
        fs::create_directories("out/tests/");

        SUBCASE("Basic") {
            for (auto storeFactory : storeFactories) {
                auto store = storeFactory();
                string key = utils::randHash(32); 

                store->insert(key, "value");
                REQUIRE(store->get(key) == "value");
                REQUIRE(store->count() == 1);

                store->update(key, "updated");
                REQUIRE(store->get(key) == "updated");

                store->remove(key);
                REQUIRE(store->count() == 0); 

                REQUIRE_THROWS(store->get(key));
            }
        }

        SUBCASE("Nulls") {
            for (auto storeFactory : storeFactories) {
                auto store = storeFactory();

                string key = utils::randHash(32); 

                store->insert(key, "hello\0world"s);
                REQUIRE(store->get(key) == "hello\0world"s);

                store->update(key, "\0goodbye\0"s);
                REQUIRE(store->get(key) == "\0goodbye\0"s);
            }
        }
    }

    TEST_CASE("Test deletes if exists") {
        fs::remove_all("out/tests");
        fs::create_directories("out/tests/");

        for (auto storeFactory : storeFactories) {
            string key = utils::randHash(32); 

            fs::remove_all(filepath);
            { // create the store, and then close it.
                auto store = storeFactory();
                REQUIRE(store->filepath == filepath);

                store->insert(key, "value");
            }
            REQUIRE(fs::exists(filepath));

            {
                auto store = storeFactory();
                REQUIRE_THROWS(store->get(key)); // We wiped the database
            }
        }
    }

    TEST_CASE("Test multiple records") {
        // Make a bunch of records. Run this last so we can examine the db manually as well.
        fs::remove_all("out/tests");
        fs::create_directories("out/tests/");

        for (auto& storeFactory : storeFactories) {
            auto store = storeFactory();
            for (int i = 0; i < 25; i++) {
                string key = utils::randHash(32);
                string value = utils::randBlob(64);
                store->insert(key, value);
                REQUIRE(store->get(key) == value);
            }
        }
    }
}