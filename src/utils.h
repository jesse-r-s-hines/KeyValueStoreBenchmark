#pragma once
#include <string>
#include <functional>
#include <filesystem>
#include <chrono>
#include <map>
#include <algorithm>
#include <type_traits>

namespace utils {
    std::string intToHex(long long i, int width);

    std::string randHash(int size);

    std::string randBlob(size_t size);

    std::chrono::nanoseconds timeIt(std::function<void()> func);

    long long diskUsage(const std::filesystem::path& filepath);

    template<typename T, typename... Ts>
    using AllSame = std::enable_if_t<std::conjunction_v<std::is_same<T, Ts>...>>;

    /**
     * Returns a new map that contains values of all the given maps.
     */
    template<typename MapLike, typename... MapLikes>
    MapLike merge(const MapLike first, const MapLikes&... rest) {
        MapLike rtrn = first;
        (std::for_each(rest.begin(), rest.end(), [&](auto& pair) { rtrn[pair.first] = pair.second; }), ...);
        return rtrn;
    }

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



