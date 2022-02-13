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
     * Can insert, update, get, and remove string keys and blob values.
     */
    class DBWrapper {
    public:
        virtual ~DBWrapper() {}

        virtual void insert(const string& key, const char* value) = 0;
        virtual void update(const string& key, const char* value) = 0;
        virtual const char* get(const string& key) = 0;
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

        void insert(const string& key, const char* value) override {
            SQLite::bind(insertStmt.value(), key, value);
            insertStmt.value().exec();
            insertStmt.value().reset();
        }

        void update(const string& key, const char* value) override {
            SQLite::bind(updateStmt.value(), value, key);
            updateStmt.value().exec();
            updateStmt.value().reset();
        }

        /**
         * Note that the returned pointer is only valid while the SQLiteWrapper object is in scope, and you haven't
         * called `get` again. (If this was actual code we'd copy it and return std::string but for benchmarking we'll
         * do the run around to avoid that extra copy of a potentially large blob)
         */
        const char* get(const string& key) override {
            getStmt.value().reset(); // reset at beginning to keep const char* alive.
            SQLite::bind(getStmt.value(), key);
            getStmt.value().executeStep(); // only one result

            const char* value = getStmt.value().getColumn(0).getText();
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

        void insert(string key, string value) override {
            // SQLITE_STATIC means that std::string is responsible for the memory of key and value
            sqlite3_bind_text(this->insertStmt, 1, key.c_str(), key.length(), SQLITE_STATIC);
            sqlite3_bind_blob(this->insertStmt, 2, value.c_str(), value.length(), SQLITE_STATIC);

            sqlite3_step(this->insertStmt);
            sqlite3_reset(this->insertStmt);
        }

        void update(string key, string value) override {
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
}
