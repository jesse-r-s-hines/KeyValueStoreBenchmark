#pragma once
#include <string>
#include <memory>
#include <map>

namespace stores {
    enum class Type {
        SQLite3, LevelDB, RocksDB, BerkeleyDB, FlatFolder
    };
    
    /** Maps all the store Types to a string name */
    const std::map<Type, std::string> types __attribute__((unused)) {
        {Type::SQLite3, "SQLite3"},
        {Type::LevelDB, "LevelDB"},
        {Type::RocksDB, "RocksDB"},
        {Type::BerkeleyDB, "BerkeleyDB"},
        {Type::FlatFolder, "FlatFolder"},
    };

    /**
     * Abstract base class for a key-value store.
     * Can insert, update, get, and remove string keys and values.
     */
    class Store {
    public:
        const std::string filepath;

        /** Get the type of the underlying store */
        virtual Type type() = 0;
        /** Get the type of the underlying store */
        std::string typeName();

        Store(const std::string& filepath);
        virtual ~Store() {}
        
        virtual void insert(const std::string& key, const std::string& value) = 0;
        virtual void update(const std::string& key, const std::string& value) = 0;
        virtual std::string get(const std::string& key) = 0;
        virtual void remove(const std::string& key) = 0;
    };

    /**
     * Factor to create a Store of the given type
     * @param type The type of store to create
     * @param filepath Where to save the store
     * @param deleteIfExists Default false. Delete the store's files if they already exist.
     */
    std::unique_ptr<Store> getStore(Type type, std::string filepath, bool deleteIfExists = false);
}
