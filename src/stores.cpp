#include <memory>
#include <filesystem>
#include <fstream>
#include <map>
#include <tuple>
#include <typeinfo>
#include <typeindex>

#include <sqlite3.h>
#include "rocksdb/db.h"
#include "leveldb/db.h"
#include <berkeleydb/include/db_cxx.h>

#include "stores.h"

namespace stores {
    namespace fs = std::filesystem;
    using fs::path;
    using std::ofstream, std::ifstream;
    using namespace std::string_literals;
    using std::string, std::to_string, std::tuple, std::function, std::unique_ptr, std::make_unique;
    using uint = unsigned int;


    class SQLite3Store : public Store {
        sqlite3* db = nullptr;
        sqlite3_stmt* insertStmt = nullptr;
        sqlite3_stmt* updateStmt = nullptr;
        sqlite3_stmt* getStmt = nullptr;
        sqlite3_stmt* removeStmt = nullptr;

        void checkStatus(int status) {
            if (status != SQLITE_OK && status != SQLITE_ROW && status != SQLITE_DONE) {
                throw std::runtime_error("SQLite error: " + std::to_string(status));
            }
        }

    public:
        SQLite3Store(const path& filepath) : Store(filepath) {
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

        ~SQLite3Store() {
            sqlite3_finalize(this->insertStmt);
            sqlite3_finalize(this->updateStmt);
            sqlite3_finalize(this->getStmt);
            sqlite3_finalize(this->removeStmt);
            sqlite3_close(this->db);
        }

        void _insert(const string& key, const string& value) override {
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

        void _update(const string& key, const string& value) override {
            int s = sqlite3_bind_text(this->updateStmt, 2, key.c_str(), key.length(), SQLITE_STATIC);
            checkStatus(s);
            s = sqlite3_bind_blob(this->updateStmt, 1, value.c_str(), value.length(), SQLITE_STATIC);
            checkStatus(s);

            s = sqlite3_step(this->updateStmt);
            checkStatus(s);
            s = sqlite3_reset(this->updateStmt);
            checkStatus(s);
        }

        string _get(const string& key) override {
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

        void _remove(const string& key) override {
            int s = sqlite3_bind_text(this->removeStmt, 1, key.c_str(), key.length(), SQLITE_STATIC);
            checkStatus(s);

            s = sqlite3_step(this->removeStmt);
            checkStatus(s);
            s = sqlite3_reset(this->removeStmt);
            checkStatus(s);
        }
    };


    class LevelDBStore : public Store {
        leveldb::DB* db;

        void checkStatus(leveldb::Status status) {
            if (!status.ok()) {
                throw std::runtime_error(status.ToString());
            }
        }

    public:
        LevelDBStore(const path& filepath) : Store(filepath) {
            fs::remove_all(filepath);
            leveldb::Options options;
            options.create_if_missing = true;
            
            leveldb::Status status = leveldb::DB::Open(options, filepath, &db);
            checkStatus(status);
        }

        ~LevelDBStore() {
            delete db;
        }

        void _insert(const string& key, const string& value) override {
           leveldb::Status s = db->Put(leveldb::WriteOptions(), key, value);
           checkStatus(s);
        }

        void _update(const string& key, const string& value) override {
            _insert(key, value);
        }

        string _get(const string& key) override {
            std::string value;
            leveldb::Status s = db->Get(leveldb::ReadOptions(), key, &value);
            checkStatus(s);
            return value;
        }

        void _remove(const string& key) override {
            leveldb::Status s = db->Delete(leveldb::WriteOptions(), key);
            checkStatus(s);
        }
    };


    class RocksDBStore : public Store {
        rocksdb::DB* db;

        void checkStatus(rocksdb::Status status) {
            if (!status.ok()) {
                throw std::runtime_error(status.ToString());
            }
        }

    public:
        RocksDBStore(const path& filepath) : Store(filepath) {
            fs::remove_all(filepath);
            rocksdb::Options options;
            options.create_if_missing = true;
            
            rocksdb::Status status = rocksdb::DB::Open(options, filepath, &db);
            checkStatus(status);
        }

        ~RocksDBStore() {
            delete db;
        }

        void _insert(const string& key, const string& value) override {
           rocksdb::Status s = db->Put(rocksdb::WriteOptions(), key, value);
           checkStatus(s);
        }

        void _update(const string& key, const string& value) override {
            _insert(key, value);
        }

        string _get(const string& key) override {
            std::string value;
            rocksdb::Status s = db->Get(rocksdb::ReadOptions(), key, &value);
            checkStatus(s);
            return value;
        }

        void _remove(const string& key) override {
            rocksdb::Status s = db->Delete(rocksdb::WriteOptions(), key);
            checkStatus(s);
        }
    };


    /**
     * See https://docs.oracle.com/cd/E17076_05/html/gsg/CXX/BerkeleyDB-Core-Cxx-GSG.pdf and
     * https://docs.oracle.com/database/bdb181/html/api_reference/CXX/frame_main.html for docs
     */
    class BerkeleyDBStore : public Store {
        Db db;

        static Dbt makeDbt(const string& str) {
            // we don't include the null terminator. We're also casting away const...
            return Dbt((void *) str.c_str(), str.size()); 
        }

        void checkStatus(int status) {
            if (status != 0) {
                throw std::runtime_error(std::to_string(status));
            }
        }

    public:
        BerkeleyDBStore(const path& filepath) : Store(filepath), db(NULL, 0) {
            fs::remove_all(filepath);
            
            int s = db.open(NULL, filepath.c_str(), NULL, DB_BTREE, DB_CREATE, 0);
            checkStatus(s);
        }

        ~BerkeleyDBStore() {
            int s = db.close(0);
            checkStatus(s);
        }

        void _insert(const string& key, const string& value) override {
            Dbt keyDbt = makeDbt(key);
            Dbt valueDbt((void *) value.c_str(), value.size());
            int s = db.put(NULL, &keyDbt, &valueDbt, 0);
            checkStatus(s);
        }

        void _update(const string& key, const string& value) override {
            _insert(key, value);
        }

        string _get(const string& key) override {
            Dbt keyDbt = makeDbt(key);
            Dbt valueDbt;
            int s = db.get(NULL, &keyDbt, &valueDbt, 0);
            checkStatus(s);
            // Note: this is a copy. See the SQLite get as well.
            return string((char*) valueDbt.get_data(), valueDbt.get_size());
        }

        void _remove(const string& key) override {
            Dbt keyDbt = makeDbt(key);
            db.del(NULL, &keyDbt, 0);
        }
    };


    /**
     * Stores each record as a file with its key as the name.
     */
    class FlatFolderStore : public Store {
        path getPath(const string& key) {
            return filepath / key;
        }

    public:
        FlatFolderStore(const path& filepath) : Store(filepath) {
            fs::remove_all(filepath);
            fs::create_directories(filepath);
        }

        void _insert(const string& key, const string& value) override {
            ofstream file(getPath(key), ofstream::out|ofstream::binary|ofstream::trunc);
            file.write(value.c_str(), value.size());
        }

        void _update(const string& key, const string& value) override {
            _insert(key, value);
        }

        string _get(const string& key) override {
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

        void _remove(const string& key) override {
            fs::remove(getPath(key));
        }
    };


    /**
     * Stores each record as a file with its key as the name. To avoid putting large numbers of files in a single
     * directory, it will nest the files like so:
     * - c4
     *     - ca
     *       - 4238a0b923820dcc509a6f75849b
     *     - ae
     *       - 728d9d4c2f636f067f89cc14862c
     * - ec
     *   - cb
     *     - c87e4b5ce2fe28308fd9f2a7baf3
     * 
     * Note: This does not hash the keys for you, and keys should be fixed width.
     * 
     * @param charsPerLevel The number of chars in each level of the name
     * @param depth The depth of the tree (0 will use all available chars)
     * @param keyLen The size of each key
     */
    class NestedFolderStore : public Store {
        uint charsPerLevel;
        uint depth;
        size_t keyLen;

        path getPath(const string& key) {
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

    public:
        NestedFolderStore(const path& filepath, uint charsPerLevel, uint depth, size_t keyLen) :
            Store(filepath),
            charsPerLevel(charsPerLevel),
            depth(depth == 0 ? keyLen / charsPerLevel + (keyLen % charsPerLevel != 0) : depth),
            keyLen(keyLen) {
            fs::remove_all(filepath);
            fs::create_directories(filepath);
        }

        void _insert(const string& key, const string& value) override {
            path path = getPath(key);
            fs::create_directories(path.parent_path());
            ofstream file(path, ofstream::out|ofstream::binary|ofstream::trunc);
            file.write(value.c_str(), value.size());
        }

        void _update(const string& key, const string& value) override {
            _insert(key, value);
        }

        string _get(const string& key) override {
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

        void _remove(const string& key) override {
            // TODO potential improvement, delete empty directories left. Though that could slow it down as well
            fs::remove(getPath(key));
        }
    };



    std::map<std::type_index, std::tuple<Type, string>> storeInfo {
        {typeid(SQLite3Store), {Type::SQLite3, "SQLite3"}},
        {typeid(LevelDBStore), {Type::LevelDB, "LevelDB"}},
        {typeid(RocksDBStore), {Type::RocksDB, "RocksDB"}},
        {typeid(BerkeleyDBStore), {Type::BerkeleyDB, "BerkeleyDB"}},
        {typeid(FlatFolderStore), {Type::FlatFolder, "FlatFolder"}},
        {typeid(NestedFolderStore), {Type::NestedFolder, "NestedFolder"}},
    };
    const std::map<Type, std::string> types = []() {
        std::map<Type, std::string> result;
        for (auto& [type, info] : storeInfo) result[std::get<0>(info)] = std::get<1>(info);
        return result;
    }();


    unique_ptr<Store> getStore(Type type, const path& filepath) {
        switch (type) {
            case Type::SQLite3:
                return make_unique<stores::SQLite3Store>(filepath);
            case Type::LevelDB:
                return make_unique<stores::LevelDBStore>(filepath);
            case Type::RocksDB:
                return make_unique<stores::RocksDBStore>(filepath);
            case Type::BerkeleyDB:
                return make_unique<stores::BerkeleyDBStore>(filepath);
            case Type::FlatFolder:
                return make_unique<stores::FlatFolderStore>(filepath);
            case Type::NestedFolder:
                // using 32 char hash (128) so we don't have to worry about collisions
                // 3 levels of nesting with 2 chars and a max of 10,000,000 records should have 2 levels with 265
                // folders and and about 142 files at the lowest level on average.
                return make_unique<stores::NestedFolderStore>(filepath, 2, 3, 32);
        }
        throw std::runtime_error("Unknown type"); // Shouldn't be possible
    };


    Store::Store(const path& filepath) : filepath(filepath) {};
    
    Type Store::type() { return std::get<0>(storeInfo.at(typeid(*this))); };
    std::string Store::typeName() { return types.at(this->type()); };
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
}
