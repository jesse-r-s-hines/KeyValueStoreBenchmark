#include <string>
#include <random>
#include <functional>
#include <chrono>
#include<iostream>

namespace helpers {
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
}



