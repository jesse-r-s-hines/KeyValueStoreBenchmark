#include <iostream>
#include <filesystem>
#include <memory>
#include <map>
#include <string>

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include "stores.h"
#include "utils.h"

namespace tests {
    namespace filesystem = std::filesystem;
    using namespace std::string_literals;
    using std::unique_ptr, std::make_unique, std::vector, std::map, std::array, std::pair, std::string;
    using filesystem::path;

    TEST_CASE("Test Stores") {
        filesystem::remove_all("out/tests");
        filesystem::create_directories("out/tests/");

        SUBCASE("Basic") {
            for (auto& [type, typeName] : stores::types) {
                auto store = stores::getStore(type, path("out") / "tests" / typeName);
                INFO(store->typeName());
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
            for (auto& [type, typeName] : stores::types) {
                auto store = stores::getStore(type, path("out") / "tests" / typeName);

                INFO(store->typeName());
                string key = utils::randHash(32); 

                store->insert(key, "hello\0world"s);
                REQUIRE(store->get(key) == "hello\0world"s);

                store->update(key, "\0goodbye\0"s);
                REQUIRE(store->get(key) == "\0goodbye\0"s);
            }
        }
    }


    TEST_CASE("Test deletes if exists") {
        filesystem::remove_all("out/tests");
        filesystem::create_directories("out/tests/");

        for (auto [type, typeName] : stores::types) {
            string filepath = path("out") / "tests" / typeName;
            INFO(filepath);
            string key = utils::randHash(32); 

            REQUIRE(!filesystem::exists(filepath));
            { // create the store, and then close it.
                auto store = stores::getStore(type, filepath);
                REQUIRE(store->filepath == filepath);

                store->insert(key, "value");
            }
            REQUIRE(filesystem::exists(filepath));

            {
                auto store = stores::getStore(type, filepath);
                REQUIRE_THROWS(store->get(key)); // We wiped the database
            }
        }
    }

    TEST_CASE("Test multiple records") {
        // Make a bunch of records. Run this last so can examine the db manually as well.
        filesystem::remove_all("out/tests");
        filesystem::create_directories("out/tests/");

        vector<unique_ptr<stores::Store>> stores{};
        for (auto [type, typeName] : stores::types)
            stores.push_back(stores::getStore(type, path("out") / "tests" / typeName));

        for (auto& store : stores) {
            for (int i = 0; i < 25; i++) {
                INFO(store->typeName());
                string key = utils::randHash(32);
                string value = utils::randBlob(64);
                store->insert(key, value);
                REQUIRE(store->get(key) == value);
            }
        }
    }
}