/**
 * Defines wrappers around each of the different storage methods with a consistent inteface.
 */
#pragma once
#include <string>
#include <memory>
#include <map>
#include <filesystem>

namespace stores {
    /** The different storage methods */
    enum class Type {
        SQLite3, LevelDB, RocksDB, BerkeleyDB, FlatFolder, NestedFolder
    };
    /** Maps all the store Types to a string name */
    extern const std::map<Type, std::string> types;

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

        /** Get the type of the underlying store */
        Type type();
        /** Get the type of the underlying store */
        std::string typeName();
        /** Current number of records in the database */
        size_t count();

        void insert(const std::string& key, const std::string& value);
        void update(const std::string& key, const std::string& value);
        std::string get(const std::string& key);
        void remove(const std::string& key);
    };

    /**
     * Factory to create a Store of the given type.
     * @param type The type of store to create
     * @param filepath Where to save the store
     */
    std::unique_ptr<Store> getStore(Type type, const std::filesystem::path& filepath);
}
