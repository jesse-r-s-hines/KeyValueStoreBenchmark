#include <string>
#include <random>
#include <functional>
#include <chrono>

#include <boost/process.hpp>

namespace utils {
    namespace process = boost::process;
    namespace chrono = std::chrono;

    std::random_device randomDevice;
    std::mt19937 randGen(randomDevice());

    template< typename T >
    std::string intToHex(T i, int width) {
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(width) << std::hex << i;
        return stream.str();
    }

    template< typename T >
    std::string intToHex(T i) {
        return intToHex(i, sizeof(T)*2);
    }

    std::string randHash() {
        std::uniform_int_distribution<long long unsigned> randNum(0, 0xFFFF'FFFF'FFFF'FFFF);
        return intToHex(randNum(randGen), 16);
    }

    std::string randBlob(size_t size) {
        std::uniform_int_distribution<unsigned char> randChar(0, 10);
        std::string blob(size, '\0');
        for (size_t i = 0; i < size; i++) {
            blob[i] = randChar(randGen);
        }
        return blob;
    }

    chrono::nanoseconds timeIt(std::function<void()> func) {
        auto start = chrono::steady_clock::now();
        func();
        auto stop = chrono::steady_clock::now();
        return chrono::duration_cast<chrono::nanoseconds>(stop - start);
    }

    /** Keeps the average and other statistics. */
    template<typename T>
    class Stats {
        long long _count = 0;
        T _sum;
        T _min;
        T _max;

    public:
        void record(T value) {
            _sum += value;
            if (_count == 0 || value < _min) _min = value;
            if (_count == 0 || value > _max) _max = value;
            _count++;
        }

        long long count() const { return _count; }
        T sum() const { return _sum; }
        T min() const { return _min; }
        T max() const { return _max; }
        /** Note: Throws divide by zero if you haven't recording anything */
        T avg() const { return _sum / _count; }
    };

    long long diskUsage(const std::string& path) {
        // TODO make a windows version of this?
        process::ipstream out;
        process::child du(process::search_path("du"), "-s", "--block-size=1", path, process::std_out > out);

        std::string outputStr;
        std::string line;
        while (du.running() && std::getline(out, line) && !line.empty())
            outputStr += line;
        du.wait();

        return std::stol(outputStr);
    }
}



