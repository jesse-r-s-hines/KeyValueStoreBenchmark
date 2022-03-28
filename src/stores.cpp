#include <memory>
#include <filesystem>
#include <fstream>

#include "stores.h"

namespace stores {
    namespace fs = std::filesystem;
    using fs::path;
    using std::ofstream, std::ifstream;
    using namespace std::string_literals;
    using std::string, std::to_string, std::tuple, std::function, std::unique_ptr, std::make_unique;
    using uint = unsigned int;



    Store::Store(const path& filepath) : filepath(filepath) {};
    
    size_t Store::count() { return _count; };

    void Store::insert(const string& key, const string& value) {
        this->_insert(key, value);
        _count++;
    };
    void Store::update(const string& key, const string& value) { this->_update(key, value); };
    string Store::get(const string& key) { return this->_get(key); };
    void Store::remove(const string& key) {
        this->_remove(key);
        _count--;
    };



    SQLite3Store::SQLite3Store(const path& filepath) : Store(filepath) {
        fs::remove_all(filepath);
        int s = sqlite3_open(filepath.c_str(), &db);
        checkStatus(s);
        char* errMmsg = nullptr;

        string sql = 
            "CREATE TABLE IF NOT EXISTS data("
            "    key TEXT PRIMARY KEY NOT NULL,"
            "    value BLOB NOT NULL"
            ");";
        s = sqlite3_exec(this->db, sql.c_str(), nullptr, 0, &errMmsg);
        checkStatus(s);

        sql = "INSERT INTO data VALUES (?, ?)";
        s = sqlite3_prepare_v2(this->db, sql.c_str(), sql.length(), &(this->insertStmt), nullptr);
        checkStatus(s);

        sql = "UPDATE data SET value = ? WHERE key = ?";
        s = sqlite3_prepare_v2(this->db, sql.c_str(), sql.length(), &(this->updateStmt), nullptr);
        checkStatus(s);

        sql = "SELECT value FROM data WHERE key = ?";
        s = sqlite3_prepare_v2(this->db, sql.c_str(), sql.length(), &(this->getStmt), nullptr);
        checkStatus(s);

        sql = "DELETE FROM data WHERE key = ?";
        s = sqlite3_prepare_v2(this->db, sql.c_str(), sql.length(), &(this->removeStmt), nullptr);
        checkStatus(s);
    }

    SQLite3Store::~SQLite3Store() {
        sqlite3_finalize(this->insertStmt);
        sqlite3_finalize(this->updateStmt);
        sqlite3_finalize(this->getStmt);
        sqlite3_finalize(this->removeStmt);
        sqlite3_close(this->db);
    }

    void SQLite3Store::checkStatus(int status) {
        if (status != SQLITE_OK && status != SQLITE_ROW && status != SQLITE_DONE) {
            throw std::runtime_error("SQLite error: " + std::to_string(status));
        }
    }

    void SQLite3Store::_insert(const string& key, const string& value) {
        // SQLITE_STATIC means that std::string is responsible for the memory of key and value
        int s = sqlite3_bind_text(this->insertStmt, 1, key.c_str(), key.length(), SQLITE_STATIC);
        checkStatus(s);
        s = sqlite3_bind_blob(this->insertStmt, 2, value.c_str(), value.length(), SQLITE_STATIC);
        checkStatus(s);

        s = sqlite3_step(this->insertStmt);
        checkStatus(s);
        s = sqlite3_reset(this->insertStmt);
        checkStatus(s);
    }

    void SQLite3Store::_update(const string& key, const string& value) {
        int s = sqlite3_bind_text(this->updateStmt, 2, key.c_str(), key.length(), SQLITE_STATIC);
        checkStatus(s);
        s = sqlite3_bind_blob(this->updateStmt, 1, value.c_str(), value.length(), SQLITE_STATIC);
        checkStatus(s);

        s = sqlite3_step(this->updateStmt);
        checkStatus(s);
        s = sqlite3_reset(this->updateStmt);
        checkStatus(s);
    }

    string SQLite3Store::_get(const string& key) {
        int s = sqlite3_bind_text(this->getStmt, 1, key.c_str(), key.length(), SQLITE_STATIC);
        checkStatus(s);
        s = sqlite3_step(this->getStmt);
        if (s == SQLITE_DONE)
            throw std::runtime_error("Key not found");
        checkStatus(s);

        const void* valueVoid = sqlite3_column_blob(this->getStmt, 0);
        int size = sqlite3_column_bytes(this->getStmt, 0);

        // Note: This is doing a copy of the blob. the returned char* becomes invalid after calling `reset` so
        // we can't really avoid the copy? For benchmarking purposes we could potentially return a string_view or 
        // or char* and just note the pointer becomes invalid after the next call to get.
        string value(static_cast<const char*>(valueVoid), size);

        s = sqlite3_reset(this->getStmt);
        checkStatus(s);
        return value;
    }

    void SQLite3Store::_remove(const string& key) {
        int s = sqlite3_bind_text(this->removeStmt, 1, key.c_str(), key.length(), SQLITE_STATIC);
        checkStatus(s);

        s = sqlite3_step(this->removeStmt);
        checkStatus(s);
        s = sqlite3_reset(this->removeStmt);
        checkStatus(s);
    }



    LevelDBStore::LevelDBStore(const path& filepath) : Store(filepath) {
        fs::remove_all(filepath);
        leveldb::Options options;
        options.create_if_missing = true;
        
        leveldb::Status status = leveldb::DB::Open(options, filepath, &db);
        checkStatus(status);
    }

    LevelDBStore::~LevelDBStore() {
        delete db;
    }

    void LevelDBStore::checkStatus(leveldb::Status status) {
        if (!status.ok()) {
            throw std::runtime_error(status.ToString());
        }
    }

    void LevelDBStore::_insert(const string& key, const string& value) {
        leveldb::Status s = db->Put(leveldb::WriteOptions(), key, value);
        checkStatus(s);
    }

    void LevelDBStore::_update(const string& key, const string& value) {
        _insert(key, value);
    }

    string LevelDBStore::_get(const string& key) {
        std::string value;
        leveldb::Status s = db->Get(leveldb::ReadOptions(), key, &value);
        checkStatus(s);
        return value;
    }

    void LevelDBStore::_remove(const string& key) {
        leveldb::Status s = db->Delete(leveldb::WriteOptions(), key);
        checkStatus(s);
    }



    RocksDBStore::RocksDBStore(const path& filepath) : Store(filepath) {
        fs::remove_all(filepath);
        rocksdb::Options options;
        options.create_if_missing = true;
        
        rocksdb::Status status = rocksdb::DB::Open(options, filepath, &db);
        checkStatus(status);
    }

    RocksDBStore::~RocksDBStore() {
        delete db;
    }

    void RocksDBStore::checkStatus(rocksdb::Status status) {
        if (!status.ok()) {
            throw std::runtime_error(status.ToString());
        }
    }

    void RocksDBStore::_insert(const string& key, const string& value) {
        rocksdb::Status s = db->Put(rocksdb::WriteOptions(), key, value);
        checkStatus(s);
    }

    void RocksDBStore::_update(const string& key, const string& value) {
        _insert(key, value);
    }

    string RocksDBStore::_get(const string& key) {
        std::string value;
        rocksdb::Status s = db->Get(rocksdb::ReadOptions(), key, &value);
        checkStatus(s);
        return value;
    }

    void RocksDBStore::_remove(const string& key) {
        rocksdb::Status s = db->Delete(rocksdb::WriteOptions(), key);
        checkStatus(s);
    }



    BerkeleyDBStore::BerkeleyDBStore(const path& filepath) : Store(filepath), db(NULL, 0) {
        fs::remove_all(filepath);
        
        int s = db.open(NULL, filepath.c_str(), NULL, DB_BTREE, DB_CREATE, 0);
        checkStatus(s);
    }

    BerkeleyDBStore::~BerkeleyDBStore() {
        int s = db.close(0);
        checkStatus(s);
    }

    Dbt BerkeleyDBStore::makeDbt(const string& str) {
        // we don't include the null terminator. We're also casting away const...
        return Dbt((void *) str.c_str(), str.size()); 
    }
    
    void BerkeleyDBStore::checkStatus(int status) {
        if (status != 0) {
            throw std::runtime_error(std::to_string(status));
        }
    }

    void BerkeleyDBStore::_insert(const string& key, const string& value) {
        Dbt keyDbt = makeDbt(key);
        Dbt valueDbt((void *) value.c_str(), value.size());
        int s = db.put(NULL, &keyDbt, &valueDbt, 0);
        checkStatus(s);
    }

    void BerkeleyDBStore::_update(const string& key, const string& value) {
        _insert(key, value);
    }

    string BerkeleyDBStore::_get(const string& key) {
        Dbt keyDbt = makeDbt(key);
        Dbt valueDbt;
        int s = db.get(NULL, &keyDbt, &valueDbt, 0);
        checkStatus(s);
        // Note: this is a copy. See the SQLite get as well.
        return string((char*) valueDbt.get_data(), valueDbt.get_size());
    }

    void BerkeleyDBStore::_remove(const string& key) {
        Dbt keyDbt = makeDbt(key);
        db.del(NULL, &keyDbt, 0);
    }



    FlatFolderStore::FlatFolderStore(const path& filepath) : Store(filepath) {
        fs::remove_all(filepath);
        fs::create_directories(filepath);
    }

    path FlatFolderStore::getPath(const string& key) {
        return filepath / key;
    }

    void FlatFolderStore::_insert(const string& key, const string& value) {
        ofstream file(getPath(key), ofstream::out|ofstream::binary|ofstream::trunc);
        file.write(value.c_str(), value.size());
    }

    void FlatFolderStore::_update(const string& key, const string& value) {
        _insert(key, value);
    }

    string FlatFolderStore::_get(const string& key) {
        ifstream file(getPath(key), ifstream::in|ifstream::binary|ifstream::ate); // open at end of file
        if (!file.is_open())
            throw std::runtime_error("Key \""s + key + "\" doesn't exit");

        // get pos (which is size of file since we're at the end)
        // would building a buffer be better as it wouldn't have a second seek? (but we'd do more memory allocations)
        auto size = file.tellg();
        file.seekg(0, ifstream::beg);
        string value;
        value.resize(size);
        file.read(&value[0], size);
        return value;
    }

    void FlatFolderStore::_remove(const string& key) {
        fs::remove(getPath(key));
    }



    NestedFolderStore::NestedFolderStore(const path& filepath, uint charsPerLevel, uint depth, size_t keyLen) :
        Store(filepath),
        charsPerLevel(charsPerLevel),
        depth(depth == 0 ? keyLen / charsPerLevel + (keyLen % charsPerLevel != 0) : depth),
        keyLen(keyLen) {
        fs::remove_all(filepath);
        fs::create_directories(filepath);
    }

    path NestedFolderStore::getPath(const string& key) {
        if (key.size() != keyLen)
            throw std::runtime_error("Key \"" + key + "\" not of size " + to_string(keyLen));

        path recordPath(filepath);
        uint i = 0;
        for (; i < (depth - 1) * charsPerLevel; i += charsPerLevel) {
            recordPath /= key.substr(i, charsPerLevel); // substr does bounds check
        }
        if (i < key.size()) {
            recordPath /= key.substr(i, string::npos);
        }

        return recordPath;
    }

    void NestedFolderStore::_insert(const string& key, const string& value) {
        path path = getPath(key);
        fs::create_directories(path.parent_path());
        ofstream file(path, ofstream::out|ofstream::binary|ofstream::trunc);
        file.write(value.c_str(), value.size());
    }

    void NestedFolderStore::_update(const string& key, const string& value) {
        _insert(key, value);
    }

    string NestedFolderStore::_get(const string& key) {
        ifstream file(getPath(key), ifstream::in|ifstream::binary|ifstream::ate); // open at end of file
        if (!file.is_open())
            throw std::runtime_error("Key \""s + key + "\" doesn't exit");

        // get pos (which is size of file since we're at the end)
        // would building a buffer be better as it wouldn't have a second seek? (but we'd do more memory allocations)
        auto size = file.tellg();
        file.seekg(0, ifstream::beg);
        string value;
        value.resize(size);
        file.read(&value[0], size);
        return value;
    }

    void NestedFolderStore::_remove(const string& key) {
        // TODO potential improvement, delete empty directories
        fs::remove(getPath(key));
    }
}
