#pragma once
#include <string>
#include <functional>
#include <filesystem>
#include <chrono>
#include <map>
#include <algorithm>
#include <type_traits>
#include <random>

namespace utils {
    std::string intToHex(long long i, int width);

    template<typename T>
    struct Range { T min; T max; };

    extern std::random_device randomDevice;
    extern std::mt19937 randGen;

    /** Random int in range on interval (inclusive) */
    template<typename T>
    T randInt(T min, T max) {
        std::uniform_int_distribution<T> randRange(min, max);
        return randRange(randGen);
    }

    std::string randHash(int size);

    std::string randBlob(size_t size);


    std::chrono::nanoseconds timeIt(std::function<void()> func);


    const size_t KiB __attribute__((unused)) = 1024;
    const size_t MiB __attribute__((unused)) = 1024 * KiB;
    const size_t GiB __attribute__((unused)) = 1024 * MiB;

    long long diskUsage(const std::filesystem::path& filepath);

    /** Convert is in bytes to human readable string */
    std::string prettySize(std::size_t size);

    /**
     * Returns a new map that contains values of all the given maps.
     */
    template<typename MapLike, typename... MapLikes>
    MapLike merge(const MapLike first, const MapLikes&... rest) {
        MapLike rtrn = first;
        (std::for_each(rest.begin(), rest.end(), [&](auto& pair) { rtrn[pair.first] = pair.second; }), ...);
        return rtrn;
    };

    /** Keeps the average and other statistics. */
    template<typename T>
    class Stats {
        long long _count = 0;
        T _sum{};
        T _min{};
        T _max{};

    public:
        Stats() {}
        /** Constructs and then records each record in records */
        Stats(std::initializer_list<T> records) : Stats() {
            this->recordAll(records);
        }

        void record(T value) {
            _sum += value;
            if (_count == 0 || value < _min) _min = value;
            if (_count == 0 || value > _max) _max = value;
            _count++;
        }

        template<typename Iterable>
        void recordAll(Iterable records) {
            for (T record : records) this->record(record);
        }

        long long count() const { return _count; }
        T sum() const { return _sum; }
        T min() const { return _min; }
        T max() const { return _max; }
        /** Note: Throws divide by zero if you haven't recording anything */
        T avg() const { return _sum / _count; }
    };
}



