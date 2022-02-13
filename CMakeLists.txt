cmake_minimum_required(VERSION 3.18)
project(benchmark)

find_package(SQLiteCpp REQUIRED)
find_package(RocksDB REQUIRED)
find_package(leveldb REQUIRED)

# add the executable
add_executable(benchmark src/main.cpp)
set_property(TARGET benchmark PROPERTY CXX_STANDARD 17)
# GCC specific
target_compile_options(benchmark PRIVATE -Wall -Wextra -pedantic -O2)

target_link_libraries(benchmark
    PRIVATE
        SQLiteCpp
        RocksDB::rocksdb
        leveldb::leveldb
        ${CMAKE_BINARY_DIR}/berkeleydb/lib/libdb_cxx.a
        ${CMAKE_BINARY_DIR}/berkeleydb/lib/libdb_stl.a
)

target_include_directories(benchmark PRIVATE build)
