#include <optional>

#include <SQLiteCpp/SQLiteCpp.h>
#include <SQLiteCpp/VariadicBind.h>
#include "rocksdb/db.h"
#include "leveldb/db.h"
#include <berkeleydb/include/db_cxx.h>

namespace dbwrappers {
    using std::string, std::optional;

    /**
     * Abstract base class for the other DB wrappers.
     * 
     * Can insert, update, get, and remove string keys and values.
     */
    class DBWrapper {
    public:
        virtual ~DBWrapper() {}

        /** Returns the name of the underlying database */
        virtual string type() = 0;

        virtual void insert(const string& key, const string& value) = 0;
        virtual void update(const string& key, const string& value) = 0;
        virtual string get(const string& key) = 0;
        virtual void remove(const string& key) = 0;
    };


    class SQLiteWrapper : public DBWrapper {
        SQLite::Database db;
        // because of https://github.com/SRombauts/SQLiteCpp/issues/347 we need to use optional
        optional<SQLite::Statement> insertStmt;
        optional<SQLite::Statement> updateStmt;
        optional<SQLite::Statement> getStmt;
        optional<SQLite::Statement> removeStmt;

    public:
        SQLiteWrapper(const string& filename) :
            db(filename, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE) {

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

        string type() override { return "sqlite3"; }

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
            // something and just note the pointer becomes invalid after next call to get. Should probably use string_view
            // everywhere to avoid copies? Though reference already should avoid copy unless I'm passing char*
            const string& value = getStmt.value().getColumn(0).getText();
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
    class SQLiteWrapper : public DB {
        sqlite3* db = nullptr;
        sqlite3_stmt* insertStmt = nullptr;
        sqlite3_stmt* updateStmt = nullptr;
        sqlite3_stmt* getStmt = nullptr;
        sqlite3_stmt* removeStmt = nullptr;

    public:
        SQLiteWrapper(string filename) {
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

        ~SQLiteWrapper() {
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

    class LevelDBWrapper : public DBWrapper {
        leveldb::DB* db;

        void checkStatus(leveldb::Status status) {
            if (!status.ok()) {
                throw std::runtime_error(status.ToString());
            }
        }

    public:
        LevelDBWrapper(const string& filename) {
            leveldb::Options options;
            options.create_if_missing = true;
            
            leveldb::Status status = leveldb::DB::Open(options, filename, &db);
            checkStatus(status);
        }

        ~LevelDBWrapper() {
            delete db;
        }

        string type() override { return "leveldb"; }


        void insert(const string& key, const string& value) override {
           leveldb::Status s = db->Put(leveldb::WriteOptions(), key, value);
           checkStatus(s);
        }

        void update(const string& key, const string& value) override {
            leveldb::Status s = db->Put(leveldb::WriteOptions(), key, value);
            checkStatus(s);
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

    class RocksDBWrapper : public DBWrapper {
        rocksdb::DB* db;

        void checkStatus(rocksdb::Status status) {
            if (!status.ok()) {
                throw std::runtime_error(status.ToString());
            }
        }

    public:
        RocksDBWrapper(const string& filename) {
            rocksdb::Options options;
            options.create_if_missing = true;
            
            rocksdb::Status status = rocksdb::DB::Open(options, filename, &db);
            checkStatus(status);
        }

        ~RocksDBWrapper() {
            delete db;
        }

        string type() override { return "rocksdb"; }


        void insert(const string& key, const string& value) override {
           rocksdb::Status s = db->Put(rocksdb::WriteOptions(), key, value);
           checkStatus(s);
        }

        void update(const string& key, const string& value) override {
            rocksdb::Status s = db->Put(rocksdb::WriteOptions(), key, value);
            checkStatus(s);
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
}
