#include <string>
#include <random>
#include <functional>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sys/resource.h>

#include <boost/process.hpp>
#include <boost/uuid/detail/sha1.hpp>

#include "utils.h"

namespace utils {
    using std::string, std::filesystem::path, std::vector, std::ofstream, std::ifstream;
    namespace process = boost::process;
    namespace chrono = std::chrono;
    namespace filesystem = std::filesystem;
    using boost::uuids::detail::sha1;

    std::random_device randomDevice;
    std::mt19937 randGen(randomDevice());

    string randBlob(size_t size) {
        std::uniform_int_distribution<unsigned char> randChar(0, 0xFF);
        string blob;
        blob.resize(size);
        std::generate(blob.begin(), blob.end(), [&]() { return randChar(randGen); });
        return blob;
    }

    string randBlob(Range<size_t> size) {
        return randBlob(randInt(size.min, size.max));
    }

    ClobGenerator::ClobGenerator(const filesystem::path& textFolder) : textFolder(textFolder), filesTotalSize(0) {
        // Cache the file list
        for (auto file : filesystem::directory_iterator(this->textFolder)) {
            if (file.path().extension() == ".txt") {
                this->fileSizes.push_back({file.path(), file.file_size()});
                filesTotalSize += file.file_size();
            }
        }
    }

    string ClobGenerator::operator()(size_t size) {
        // Pick an evenly distributed substr of all the files by conceptually concatenating them all then picking a
        // random substr from that. We need the sizes of all files to do that without actually reading all in.
        // This implementation will probably have issues with variable width encodings (UTF-8)...
        size_t start = randInt<size_t>(0, this->filesTotalSize - size);
        size_t end = start + size;

        size_t pos = 0;
        auto fileInfo = this->fileSizes.begin();
        for (; pos + fileInfo->size < start; fileInfo++) {
            pos += fileInfo->size;
        }

        string clob(size, '\0');
        while (pos < end) {
            ifstream file(fileInfo->file, ifstream::in|ifstream::binary);
            if (pos < start) {
                file.seekg(start - pos);
                pos = start;
            }

            auto bufferPos = pos - start;
            auto remainingFile = fileInfo->size - file.tellg();
            file.read(&clob[bufferPos], std::min(remainingFile, size - bufferPos)); // read up to EOF or end of buffer

            pos += remainingFile;
            fileInfo++;
        }

        return clob;
    }

    string ClobGenerator::operator()(Range<size_t> size){
        return (*this)(randInt(size.min, size.max));
    }

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

    std::string genKey(size_t i) {
        sha1 hash;
        hash.process_bytes(reinterpret_cast<void*>(&i), sizeof(i));
        hash.process_byte(136); // An arbitrary salt
        sha1::digest_type digest;
        hash.get_digest(digest);

        std::stringstream ss;
        for (auto part : digest)
            ss << std::setfill('0') << std::setw(sizeof(part) * 2) << std::hex << part;
        return ss.str().substr(0, 32);
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
        while (out && std::getline(out, line) && !line.empty())
            outputStr += line + "\n";
        du.wait();

        return std::stol(outputStr);
    }

    size_t getPeakMemUsage() {
        // See https://man7.org/linux/man-pages/man2/getrusage.2.html
        // Could also parse /proc/[pid]/status (see https://man7.org/linux/man-pages/man5/proc.5.html)
        struct rusage info;
        getrusage(RUSAGE_SELF, &info);
        return info.ru_maxrss; // ru_maxrss is the resident set size in kB
    }

    void resetPeakMemUsage() {
        // See https://man7.org/linux/man-pages/man5/proc.5.html
        ofstream clearRefs("/proc/self/clear_refs");
        clearRefs << "5";
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



