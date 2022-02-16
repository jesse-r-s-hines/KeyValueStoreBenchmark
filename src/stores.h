#pragma once
#include <string>
#include <filesystem>
#include <memory>

namespace stores {
    /**
     * Abstract base class for a key-value store.
     * Can insert, update, get, and remove string keys and values.
     */
    class Store {
    public:
        const std::string filepath;
        /** Name of the underlying store */
        const std::string type;

        Store(const std::string& filepath, const std::string& type);
        virtual ~Store() {}

        virtual void insert(const std::string& key, const std::string& value) = 0;
        virtual void update(const std::string& key, const std::string& value) = 0;
        virtual std::string get(const std::string& key) = 0;
        virtual void remove(const std::string& key) = 0;
    };

    // enum class StoreType {
    //     sqlite3,
    //     leveldb,
    //     rocksdb,
    //     berkeleydb
    // };
    // static const std::string StoreTypeNames[] = {
    //     "sqlite3",
    //     "leveldb",
    //     "rocksdb",
    //     "berkeleydb"
    // };
    
    /**
     * Factor to create a Store of the given type
     * @param type The type of store to create
     * @param filepath Where to save the store
     * @param deleteIfExists Default false. Delete the store's files if they already exist.
     */
    std::unique_ptr<Store> getStore(std::string type, std::filesystem::path filepath, bool deleteIfExists = false);
}
