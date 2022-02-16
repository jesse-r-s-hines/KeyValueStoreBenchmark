#include <optional>
#include <memory>

#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/VariadicBind.h>
#include "rocksdb/db.h"
#include "leveldb/db.h"
#include <berkeleydb/include/db_cxx.h>

#include "stores.h"

namespace stores {
    using std::string, std::optional, std::unique_ptr, std::make_unique;
    using namespace std::string_literals;

    Store::Store(const string& filepath, const string& type) : filepath(filepath), type(type) {} 

    class SQLite3Store : public Store {
        SQLite::Database db;
        // because of https://github.com/SRombauts/SQLiteCpp/issues/347 we need to use optional
        optional<SQLite::Statement> insertStmt;
        optional<SQLite::Statement> updateStmt;
        optional<SQLite::Statement> getStmt;
        optional<SQLite::Statement> removeStmt;

    public:
        SQLite3Store(const string& filepath) : Store(filepath, "sqlite3"),
            db(filepath, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE) {

            string sql = 
                "CREATE TABLE IF NOT EXISTS data("
                "    key TEXT PRIMARY KEY NOT NULL,"
                "    value BLOB NOT NULL"
                ");";
            db.exec(sql);

            insertStmt.emplace(db, "INSERT INTO data VALUES (?, ?)");
            updateStmt.emplace(db, "UPDATE data SET value = ? WHERE key = ?");
            getStmt.emplace(db, "SELECT value FROM data WHERE key = ?");
            removeStmt.emplace(db, "DELETE FROM data WHERE key = ?");
        }

        void insert(const string& key, const string& value) override {
            SQLite::bind(insertStmt.value(), key, value);
            insertStmt.value().exec();
            insertStmt.value().reset();
        }

        void update(const string& key, const string& value) override {
            SQLite::bind(updateStmt.value(), value, key);
            updateStmt.value().exec();
            updateStmt.value().reset();
        }

        string get(const string& key) override {
            SQLite::bind(getStmt.value(), key);
            getStmt.value().executeStep(); // only one result

            // Note: This is doing a copy of the blob. the returned char* becomes invalid after calling `reset` so
            // we can't really avoid the copy? For benchmarking purposes we could potentially return a string_view or 
            // or char* and just note the pointer becomes invalid after next call to get. Should probably use string_view
            // everywhere to avoid copies? Though reference already should avoid copy unless I'm passing char*
            string value = getStmt.value().getColumn(0).getString();
            getStmt.value().reset();
            return value;
        }

        void remove(const string& key) override {
            SQLite::bind(removeStmt.value(), key);
            removeStmt.value().exec();
            removeStmt.value().reset();
        }
    };

    /*
    class SQLite3Store : public DB {
        sqlite3* db = nullptr;
        sqlite3_stmt* insertStmt = nullptr;
        sqlite3_stmt* updateStmt = nullptr;
        sqlite3_stmt* getStmt = nullptr;
        sqlite3_stmt* removeStmt = nullptr;

    public:
        SQLite3Store(string filename) {
            sqlite3_open(filename.c_str(), &db);
            char* errMmsg = nullptr;

            string sql = 
                "CREATE TABLE IF NOT EXISTS data("
                "    key TEXT PRIMARY KEY NOT NULL,"
                "    value BLOB NOT NULL"
                ");";
            sqlite3_exec(this->db, sql.c_str(), nullptr, 0, &errMmsg);

            sql = "INSERT INTO data VALUES (?, ?)";
            sqlite3_prepare_v2(this->db, sql.c_str(), sql.length(), &(this->insertStmt), nullptr);

            sql = "UPDATE data SET value = ?, WHERE key = ?";
            sqlite3_prepare_v2(this->db, sql.c_str(), sql.length(), &(this->updateStmt), nullptr);

            sql = "SELECT value FROM data WHERE key = ?";
            sqlite3_prepare_v2(this->db, sql.c_str(), sql.length(), &(this->getStmt), nullptr);

            sql = "DELETE FROM data WHERE key = ?";
            sqlite3_prepare_v2(this->db, sql.c_str(), sql.length(), &(this->removeStmt), nullptr);
        }

        ~SQLite3Store() {
            sqlite3_close(this->db);
            sqlite3_finalize(this->insertStmt);
            sqlite3_finalize(this->updateStmt);
            sqlite3_finalize(this->getStmt);
            sqlite3_finalize(this->removeStmt);
        }

        void insert(string key, const string& value) override {
            // SQLITE_STATIC means that std::string is responsible for the memory of key and value
            sqlite3_bind_text(this->insertStmt, 1, key.c_str(), key.length(), SQLITE_STATIC);
            sqlite3_bind_blob(this->insertStmt, 2, value.c_str(), value.length(), SQLITE_STATIC);

            sqlite3_step(this->insertStmt);
            sqlite3_reset(this->insertStmt);
        }

        void update(string key, const string& value) override {
            sqlite3_bind_text(this->updateStmt, 2, key.c_str(), key.length(), SQLITE_STATIC);
            sqlite3_bind_blob(this->updateStmt, 1, value.c_str(), value.length(), SQLITE_STATIC);

            sqlite3_step(this->updateStmt);
            sqlite3_reset(this->updateStmt);
        }

        string get(string key) override {
            sqlite3_bind_text(this->getStmt, 1, key.c_str(), key.length(), SQLITE_STATIC);

            sqlite3_step(this->getStmt);
            const void* temp = sqlite3_column_blob(this->getStmt, 1);
            sqlite3_reset(this->getStmt);
        }

        void remove(string key) override {
            sqlite3_bind_text(this->removeStmt, 1, key.c_str(), key.length(), SQLITE_STATIC);

            sqlite3_step(this->removeStmt);
            sqlite3_reset(this->removeStmt);
        }
    };
    */

    class LevelDBStore : public Store {
        leveldb::DB* db;

        void checkStatus(leveldb::Status status) {
            if (!status.ok()) {
                throw std::runtime_error(status.ToString());
            }
        }

    public:
        LevelDBStore(const string& filepath) : Store(filepath, "leveldb") {
            leveldb::Options options;
            options.create_if_missing = true;
            
            leveldb::Status status = leveldb::DB::Open(options, filepath, &db);
            checkStatus(status);
        }

        ~LevelDBStore() {
            delete db;
        }

        void insert(const string& key, const string& value) override {
           leveldb::Status s = db->Put(leveldb::WriteOptions(), key, value);
           checkStatus(s);
        }

        void update(const string& key, const string& value) override {
            insert(key, value);
        }

        string get(const string& key) override {
            std::string value;
            leveldb::Status s = db->Get(leveldb::ReadOptions(), key, &value);
            checkStatus(s);
            return value;
        }

        void remove(const string& key) override {
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
        RocksDBStore(const string& filepath) : Store(filepath, "rocksdb") {
            rocksdb::Options options;
            options.create_if_missing = true;
            
            rocksdb::Status status = rocksdb::DB::Open(options, filepath, &db);
            checkStatus(status);
        }

        ~RocksDBStore() {
            delete db;
        }

        void insert(const string& key, const string& value) override {
           rocksdb::Status s = db->Put(rocksdb::WriteOptions(), key, value);
           checkStatus(s);
        }

        void update(const string& key, const string& value) override {
            insert(key, value);
        }

        string get(const string& key) override {
            std::string value;
            rocksdb::Status s = db->Get(rocksdb::ReadOptions(), key, &value);
            checkStatus(s);
            return value;
        }

        void remove(const string& key) override {
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
        BerkeleyDBStore(const string& filepath) : Store(filepath, "berkeleydb"), db(NULL, 0) {
            int s = db.open(NULL, filepath.c_str(), NULL, DB_BTREE, DB_CREATE, 0);
            checkStatus(s);
        }

        ~BerkeleyDBStore() {
            int s = db.close(0);
            checkStatus(s);
        }

        void insert(const string& key, const string& value) override {
            Dbt keyDbt = makeDbt(key);
            Dbt valueDbt((void *) value.c_str(), value.size());
            int s = db.put(NULL, &keyDbt, &valueDbt, 0);
            checkStatus(s);
        }

        void update(const string& key, const string& value) override {
            insert(key, value);
        }

        string get(const string& key) override {
            Dbt keyDbt = makeDbt(key);
            Dbt valueDbt;
            int s = db.get(NULL, &keyDbt, &valueDbt, 0);
            checkStatus(s);
            // Note: this is a copy. See the SQLite get as well.
            return string((char*) valueDbt.get_data(), valueDbt.get_size());
        }

        void remove(const string& key) override {
            Dbt keyDbt = makeDbt(key);
            db.del(NULL, &keyDbt, 0);
        }
    };

    unique_ptr<Store> getStore(std::string type, std::filesystem::path filepath, bool deleteIfExists) {
        if (type == "sqlite3") {
            return make_unique<stores::SQLite3Store>(filepath);
        } else if (type == "leveldb") {
            return make_unique<stores::LevelDBStore>(filepath);
        } else if (type == "rocksdb") {
            return make_unique<stores::RocksDBStore>(filepath);
        } else if (type == "berkeleydb") {
            return make_unique<stores::BerkeleyDBStore>(filepath);
        } else {
            throw std::runtime_error("Unknown type: "s + type);
        }
    }
}
