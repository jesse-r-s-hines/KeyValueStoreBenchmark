#include <iostream>
#include <filesystem>
#include <random>
#include <memory>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "dbwrappers.cpp"

using std::unique_ptr, std::make_unique, std::exception, std::runtime_error,
      std::vector, std::array, std::string, std::size_t;

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

    array<unique_ptr<dbwrappers::DBWrapper>, 4> dbs{
        make_unique<dbwrappers::SQLiteWrapper>("dbs/sqlite3.db"),
        make_unique<dbwrappers::LevelDBWrapper>("dbs/leveldb.db"),
        make_unique<dbwrappers::RocksDBWrapper>("dbs/rocksdb.db"),
        make_unique<dbwrappers::BerkeleyDBWraper>("dbs/berkleydb.db"),
    };

    for (auto& db : dbs) {
        db->insert("key", "value");
        CHECK(db->get("key") == "value");

        db->update("key", "updated");
        CHECK(db->get("key") == "updated");

        db->remove("key");

        CHECK_THROWS(db->get("key"));
    }
}

// int main() {
//     std::filesystem::remove_all("dbs");
//     std::filesystem::create_directory("dbs");

//     dbwrappers::SQLiteWrapper sqliteDB("dbs/sqlite3.db");
//     testDB(sqliteDB);

//     dbwrappers::LevelDBWrapper leveldbDB("dbs/leveldb.db");
//     testDB(leveldbDB);

//     dbwrappers::RocksDBWrapper rocksdbDB("dbs/rocksdb.db");
//     testDB(rocksdbDB);

//     dbwrappers::BerkeleyDBWraper berkleydbDB("dbs/berkleydb.db");
//     testDB(berkleydbDB);
// }