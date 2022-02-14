#include <string>
#include <random>
#include <functional>
#include <chrono>

namespace helpers {
    std::random_device randomDevice;
    std::mt19937 randGen(randomDevice());

    std::string randBlob(size_t size) {
        std::uniform_int_distribution<unsigned char> randChar(0, 10);
        std::string blob(size, '\0');
        for (size_t i = 0; i < size; i++) {
            blob[i] = randChar(randGen);
        }
        return blob;
    }

    std::chrono::nanoseconds timeFunc(std::function<void()> func) {
        auto start = std::chrono::steady_clock::now();
        func();
        auto stop = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
    }
}



