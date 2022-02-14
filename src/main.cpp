#include <iostream>
#include <filesystem>
#include <random>

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

int main() {
    std::filesystem::remove_all("dbs");
    std::filesystem::create_directory("dbs");

    dbwrappers::SQLiteWrapper sqliteDB("dbs/sqlite3.db");
    testDB(sqliteDB);

    dbwrappers::LevelDBWrapper leveldbDB("dbs/leveldb.db");
    testDB(leveldbDB);

    dbwrappers::RocksDBWrapper rocksdbDB("dbs/rocksdb.db");
    testDB(rocksdbDB);

    dbwrappers::BerkeleyDBWraper berkleydbDB("dbs/berkleydb.db");
    testDB(berkleydbDB);
}