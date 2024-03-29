cmake_minimum_required(VERSION 3.18)
project(benchmark)

find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(RocksDB REQUIRED)
find_package(leveldb REQUIRED)
find_package(doctest REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)

# add the executable
add_executable(benchmark src/main.cpp src/stores.cpp src/utils.cpp)
set_property(TARGET benchmark PROPERTY CXX_STANDARD 17)
# GCC specific
target_compile_options(benchmark PRIVATE -Wall -Wextra -pedantic -O2)

target_link_libraries(benchmark
    PRIVATE
        unofficial::sqlite3::sqlite3
        RocksDB::rocksdb
        leveldb::leveldb
        ${CMAKE_BINARY_DIR}/berkeleydb/lib/libdb_cxx.a
        ${CMAKE_BINARY_DIR}/berkeleydb/lib/libdb_stl.a
        doctest::doctest
        ${Boost_LIBRARIES}
)

target_include_directories(benchmark PRIVATE build)
target_include_directories(benchmark PRIVATE src)
