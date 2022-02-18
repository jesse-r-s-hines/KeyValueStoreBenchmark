#include <string>
#include <random>
#include <functional>
#include <chrono>
#include <iomanip>

#include <boost/process.hpp>

#include "utils.h"

namespace utils {
    using std::string;
    namespace process = boost::process;
    namespace chrono = std::chrono;

    std::random_device randomDevice;
    std::mt19937 randGen(randomDevice());

    template< typename T >
    string intToHex(T i, int width) {
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(width) << std::hex << i;
        return stream.str();
    }

    template< typename T >
    string intToHex(T i) {
        return intToHex(i, sizeof(T)*2);
    }

    string randHash(int size) {
        std::uniform_int_distribution<unsigned char> randNibble(0x0, 0xF);
        string hash;
        hash.resize(size);
        std::generate(hash.begin(), hash.end(), [&]() { return intToHex(randNibble(randGen), 2)[0]; });
        return hash;
    }

    string randBlob(size_t size) {
        std::uniform_int_distribution<unsigned char> randChar(0, 0xFF);
        string blob;
        blob.resize(size);
        std::generate(blob.begin(), blob.end(), [&]() { return randChar(randGen); });
        return blob;
    }

    chrono::nanoseconds timeIt(std::function<void()> func) {
        auto start = chrono::steady_clock::now();
        func();
        auto stop = chrono::steady_clock::now();
        return chrono::duration_cast<chrono::nanoseconds>(stop - start);
    }

    long long diskUsage(const string& path) {
        // TODO make a windows version of this?
        process::ipstream out;
        process::child du(process::search_path("du"), "-s", "--block-size=1", path, process::std_out > out);

        string outputStr;
        string line;
        while (du.running() && std::getline(out, line) && !line.empty())
            outputStr += line;
        du.wait();

        return std::stol(outputStr);
    }
}



