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

        vector<unique_ptr<stores::Store>> dbs{};
        for (stores::Type type : stores::types)
            dbs.push_back(stores::getStore(type, "out/tests/"s + stores::typeNames[(int) type] + ".db", true));

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


    TEST_CASE("Test deleteIfExists") {
        filesystem::remove_all("out/tests");
        filesystem::create_directories("out/tests/");

        for (stores::Type type : stores::types) {
            string filepath = "out/tests/"s + stores::typeNames[(int) type] + ".db";
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