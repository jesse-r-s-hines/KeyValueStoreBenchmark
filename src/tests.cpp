#include <iostream>
#include <filesystem>
#include <memory>
#include <string>

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include "stores.h"

namespace tests {
    using std::unique_ptr, std::make_unique, std::vector, std::array, std::pair, std::string;
    using namespace std::string_literals;
    namespace filesystem = std::filesystem;

    TEST_CASE("Test Stores") {
        filesystem::remove_all("out/tests");
        filesystem::create_directories("out/tests/");

        vector<unique_ptr<stores::Store>> stores{};
        for (auto [type, typeName] : stores::types)
            stores.push_back(stores::getStore(type, "out/tests/"s + typeName, true));

        SUBCASE("Basic") {
            for (auto& store : stores) {
                INFO(store->typeName());

                store->insert("key", "value");
                REQUIRE(store->get("key") == "value");

                store->update("key", "updated");
                REQUIRE(store->get("key") == "updated");

                store->remove("key");

                REQUIRE_THROWS(store->get("key"));
            }
        }

        SUBCASE("Nulls") {
            for (auto& store : stores) {
                INFO(store->typeName());

                store->insert("key", "hello\0world"s);
                REQUIRE(store->get("key") == "hello\0world"s);

                store->update("key", "\0goodbye\0"s);
                REQUIRE(store->get("key") == "\0goodbye\0"s);
            }
        }
    }


    TEST_CASE("Test deleteIfExists") {
        filesystem::remove_all("out/tests");
        filesystem::create_directories("out/tests/");

        for (auto [type, typeName] : stores::types) {
            string filepath = "out/tests/"s + typeName;
            INFO(filepath);

            REQUIRE(!filesystem::exists(filepath));
            { // create the store, and then close it.
                auto store = stores::getStore(type, filepath, true);
                store->insert("key", "value");
            }
            REQUIRE(filesystem::exists(filepath));

            {
                auto store = stores::getStore(type, filepath, false);
                REQUIRE(store->get("key") == "value");
            }

            {
                auto store = stores::getStore(type, filepath, true);
                REQUIRE_THROWS(store->get("key")); // We wiped the database
            }
        }
    }
}