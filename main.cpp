#include <iostream>
#include <filesystem>

#include <sqlite3.h>
#include "rocksdb/db.h"
#include "leveldb/db.h"
#include <berkeleydb/include/db_cxx.h>

void run_sqlite3() {
    std::cout << "Sqlite version: " << sqlite3_libversion() << std::endl;

    sqlite3* mydb;
    sqlite3_open("dbs/sqlite3.db", &mydb);
    std::cout << "Database file: " << mydb << std::endl;
    sqlite3_close(mydb);
}

void run_rocksdb() {
    rocksdb::DB* db;
    std::cout << "RocksDB version: " << rocksdb::GetRocksVersionAsString() << std::endl;

    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::Status status = rocksdb::DB::Open(options, "dbs/rocksdb.db", &db);
    std::cout << "RocksDB status: " << (status.ok()) << std::endl;

    // status = db->Put(rocksdb::WriteOptions(), "key", "value");
}

void run_leveldb() {
    leveldb::DB* db;
    std::cout << "leveldb version: " << leveldb::kMajorVersion << "." << leveldb::kMinorVersion << std::endl;

    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, "dbs/leveldb.db", &db);
    std::cout << "leveldb status: " << (status.ok()) << std::endl;
}

void run_berkeleydb() {
    std::cout << "Berkeley version: " << DB_VERSION_STRING << std::endl;
    Db db(NULL, 0); // Instantiate the Db object
    u_int32_t oFlags = DB_CREATE; // Open flags;
    // Open the database
    int status = db.open(NULL, "dbs/berkeleydb.db", NULL, DB_BTREE, oFlags, 0);
    std::cout << "Berkeleydb status: " << status << "\n";
}

int main() {
    std::filesystem::remove_all("dbs");
    std::filesystem::create_directory("dbs");

    run_sqlite3();
    std::cout << std::endl;
    run_rocksdb();
    std::cout << std::endl;
    run_leveldb();
    std::cout << std::endl;
    run_berkeleydb();
}