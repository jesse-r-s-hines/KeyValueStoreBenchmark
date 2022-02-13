#include <iostream>
#include <filesystem>
#include <random>

#include <SQLiteCpp/SQLiteCpp.h>
#include "rocksdb/db.h"
#include "leveldb/db.h"
#include "berkeleydb/include/db_cxx.h"

#include "dbwrappers.cpp"

using std::exception, std::runtime_error, std::string, std::size_t;

std::random_device rd;
std::mt19937 randGen(rd());

string randBlob(size_t size) {
    std::uniform_int_distribution<unsigned char> randChar(0, 10);
    string blob(size, '\0');
    for (size_t i = 0; i < size; i++) {
        blob[i] = randChar(randGen);
    }
    return blob;
}

void testDB(dbwrappers::DBWrapper& db) {
    std::cout << "Database: " << db.type() << std::endl;
    db.insert("messsage", randBlob(10));
    db.update("messsage", "hello world");
    std::cout << "key: messsage value: " << db.get("messsage") << std::endl;;
    db.remove("messsage");

    try {
        db.get("messsage");
        std::cout << "messsage DID NOT get deleted" << std::endl;
    } catch (...) {
        std::cout << "messsage deleted" << std::endl;
    }
    std::cout << std::endl;
}

// void run_rocksdb() {
//     rocksdb::DB* db;
//     std::cout << "RocksDB version: " << rocksdb::GetRocksVersionAsString() << std::endl;

//     rocksdb::Options options;
//     options.create_if_missing = true;

//     rocksdb::Status status = rocksdb::DB::Open(options, "dbs/rocksdb.db", &db);
//     std::cout << "RocksDB status: " << (status.ok()) << std::endl;

//     // status = db->Put(rocksdb::WriteOptions(), "key", "value");
// }



// void run_berkeleydb() {
//     std::cout << "Berkeley version: " << DB_VERSION_STRING << std::endl;
//     Db db(NULL, 0); // Instantiate the Db object
//     u_int32_t oFlags = DB_CREATE; // Open flags;
//     // Open the database
//     int status = db.open(NULL, "dbs/berkeleydb.db", NULL, DB_BTREE, oFlags, 0);
//     std::cout << "Berkeleydb status: " << status << "\n";
// }

int main() {
    std::filesystem::remove_all("dbs");
    std::filesystem::create_directory("dbs");

    dbwrappers::SQLiteWrapper sqliteDB("dbs/sqlite3.db");
    testDB(sqliteDB);

    dbwrappers::LevelDBWrapper leveldbDB("dbs/leveldb.db");
    testDB(leveldbDB);

    dbwrappers::RocksDBWrapper rocksdbDB("dbs/rocksdb.db");
    testDB(rocksdbDB);
    // std::cout << std::endl;
    // run_rocksdb();
    // std::cout << std::endl;
    // run_leveldb();
    // std::cout << std::endl;
    // run_berkeleydb();
}