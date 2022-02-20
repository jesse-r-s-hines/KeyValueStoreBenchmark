#pragma once
#include <string>
#include <memory>
#include <map>
#include <filesystem>

namespace stores {
    enum class Type {
        SQLite3, LevelDB, RocksDB, BerkeleyDB, FlatFolder, NestedFolder
    };
    /** Maps all the store Types to a string name */
    extern const std::map<Type, std::string> types;

    /**
     * Abstract base class for a key-value store.
     * Can insert, update, get, and remove string keys and values.
     */
    class Store {
    public:
        const std::filesystem::path filepath;

        /** Get the type of the underlying store */
        Type type();
        /** Get the type of the underlying store */
        std::string typeName();

        Store(const std::filesystem::path& filepath);
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
    std::unique_ptr<Store> getStore(Type type, const std::filesystem::path& filepath, bool deleteIfExists = false);
}
