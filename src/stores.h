/**
 * Defines wrappers around each of the different storage methods with a consistent inteface.
 */
#pragma once
#include <string>
#include <memory>
#include <map>
#include <filesystem>

#include <sqlite3.h>
#include "rocksdb/db.h"
#include "leveldb/db.h"
#include <berkeleydb/include/db_cxx.h>

namespace stores {
    /**
     * Abstract base class for a key-value store.
     * Can insert, update, get, and remove string keys and values.
     * Keeps count of how many records are in the store.
     */
    class Store {
        size_t _count = 0;
    protected:
        // subclasses will override these.
        virtual void _insert(const std::string& key, const std::string& value) = 0;
        virtual void _update(const std::string& key, const std::string& value) = 0;
        virtual std::string _get(const std::string& key) = 0;
        virtual void _remove(const std::string& key) = 0;
        
    public:
        const std::filesystem::path filepath;

        Store(const std::filesystem::path& filepath);
        virtual ~Store() {}

        /** Current number of records in the database */
        size_t count();

        void insert(const std::string& key, const std::string& value);
        void update(const std::string& key, const std::string& value);
        std::string get(const std::string& key);
        void remove(const std::string& key);
    };

    /**
     * Wrapper around SQLite. Uses SQLite3 as a key-value store by just setting up a single table with the key as the
     * primary index.
     * See https://www.sqlite.org
     */
    class SQLite3Store : public Store {
        sqlite3* db = nullptr;
        sqlite3_stmt* insertStmt = nullptr;
        sqlite3_stmt* updateStmt = nullptr;
        sqlite3_stmt* getStmt = nullptr;
        sqlite3_stmt* removeStmt = nullptr;

        void checkStatus(int status);
    public:
        /** Create the store. Optionally pass flags from https://www.sqlite.org/c3ref/open.html */
        SQLite3Store(const std::filesystem::path& filepath, int flags = 0);

        ~SQLite3Store();

        void _insert(const std::string& key, const std::string& value) override;

        void _update(const std::string& key, const std::string& value) override;

        std::string _get(const std::string& key) override;

        void _remove(const std::string& key) override;
    };


    /**
     * Wrapper around LevelDB.
     * See https://github.com/google/leveldb
     */
    class LevelDBStore : public Store {
        leveldb::DB* db;

        void checkStatus(leveldb::Status status);

    public:
        /** Create the store. Optionally pass leveldb Options. */
        LevelDBStore(const std::filesystem::path& filepath, leveldb::Options options = {});

        ~LevelDBStore();

        void _insert(const std::string& key, const std::string& value) override;

        void _update(const std::string& key, const std::string& value) override;

        std::string _get(const std::string& key) override;

        void _remove(const std::string& key) override;
    };


    /**
     * Wrapper around RocksDB.
     * See http://rocksdb.org
     */
    class RocksDBStore : public Store {
        rocksdb::DB* db;

        void checkStatus(rocksdb::Status status);

    public:
        /** Create the store. Optionally pass rocksdb Options. */
        RocksDBStore(const std::filesystem::path& filepath, rocksdb::Options options = {});

        ~RocksDBStore();

        void _insert(const std::string& key, const std::string& value) override;

        void _update(const std::string& key, const std::string& value) override;

        std::string _get(const std::string& key) override;

        void _remove(const std::string& key) override;
    };


    /**
     * Wrapper around Berkeley DB.
     * See:
     * - https://www.oracle.com/database/technologies/related/berkeleydb.html
     * - https://docs.oracle.com/cd/E17076_05/html/gsg/CXX/BerkeleyDB-Core-Cxx-GSG.pdf
     * - https://docs.oracle.com/database/bdb181/html/api_reference/CXX/frame_main.html
     */
    class BerkeleyDBStore : public Store {
        Db db;

        static Dbt makeDbt(const std::string& str);

        void checkStatus(int status);

    public:
        /**
         * Creates the store. Optionally pass DBTYPE and flags. See
         * https://docs.oracle.com/database/bdb181/html/api_reference/CXX/frame_main.html `Db::open()`
         */
        BerkeleyDBStore(const std::filesystem::path& filepath, DBTYPE dbtype = DB_BTREE, u_int32_t flags = 0);

        ~BerkeleyDBStore();

        void _insert(const std::string& key, const std::string& value) override;

        void _update(const std::string& key, const std::string& value) override;

        std::string _get(const std::string& key) override;

        void _remove(const std::string& key) override;
    };


    /**
     * Stores each record as a file in a single folder with its key as the file name.
     */
    class FlatFolderStore : public Store {
        std::filesystem::path getPath(const std::string& key);

    public:
        FlatFolderStore(const std::filesystem::path& filepath);

        void _insert(const std::string& key, const std::string& value) override;

        void _update(const std::string& key, const std::string& value) override;

        std::string _get(const std::string& key) override;

        void _remove(const std::string& key) override;
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
     */
    class NestedFolderStore : public Store {
        uint charsPerLevel;
        uint depth;
        size_t keyLen;

        std::filesystem::path getPath(const std::string& key);

    public:
        /**
         * Create the store.
         * @param charsPerLevel The number of characters of the name used in each "level" of nesting
         * @param depth The depth of the tree (0 will use all available chars)
         * @param keyLen The size of each key (Should be at least depth * charsPerLevel)
         */
        NestedFolderStore(const std::filesystem::path& filepath, uint charsPerLevel, uint depth, size_t keyLen);

        void _insert(const std::string& key, const std::string& value) override;

        void _update(const std::string& key, const std::string& value) override;

        std::string _get(const std::string& key) override;

        void _remove(const std::string& key) override;
    };
}
