#include <iostream>
#include <filesystem>
#include <random>
#include <memory>

#include "dbwrappers.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "tests.cpp"

// std::random_device rd;
// std::mt19937 randGen(rd());

// string randBlob(size_t size) {
//     std::uniform_int_distribution<unsigned char> randChar(0, 10);
//     string blob(size, '\0');
//     for (size_t i = 0; i < size; i++) {
//         blob[i] = randChar(randGen);
//     }
//     return blob;
// }

int main(int argc, char** argv) {
    doctest::Context context;
    context.setOption("minimal", true); // only show if errors occur.
    context.applyCommandLine(argc, argv);
    int res = context.run();
    if(context.shouldExit()) return res;
}