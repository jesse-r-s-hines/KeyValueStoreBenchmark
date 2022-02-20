#include <string>
#include <random>
#include <functional>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <cmath>
#include <algorithm>

#include <boost/process.hpp>

#include "utils.h"

namespace utils {
    using std::string, std::filesystem::path, std::vector;
    namespace process = boost::process;
    namespace chrono = std::chrono;

    std::random_device randomDevice;
    std::mt19937 randGen(randomDevice());

    string intToHex(long long i, int width) {
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(width) << std::hex << i;
        return stream.str();
    }

    string randHash(int size) {
        std::uniform_int_distribution<unsigned char> randNibble(0x0, 0xF);
        string hash;
        hash.resize(size);
        for (int i = 0; i < size; i += 1)
            hash[i] = intToHex(randNibble(randGen), 1)[0];
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

    long long diskUsage(const path& filepath) {
        // TODO make a windows version of this?
        process::ipstream out;
        process::child du(process::search_path("du"), "-s", "--block-size=1", filepath.native(), process::std_out > out);

        string outputStr;
        string line;
        while (du.running() && std::getline(out, line) && !line.empty())
            outputStr += line;
        du.wait();

        return std::stol(outputStr);
    }

    string prettySize(size_t size) {
        vector<string> units{"B", "KiB", "MiB", "GiB"};
        int unitI = std::min<size_t>(std::log(size) / std::log(1024), units.size());
        int unitSize = std::pow(1024, unitI);

        double sizeInUnit = size / ((double) unitSize);
        int leftOfDecimal = std::log(sizeInUnit) / std::log(10);

        std::stringstream ss;
        ss << std::setprecision(leftOfDecimal + 2) << sizeInUnit << units[unitI];
        return ss.str();
    }
}



