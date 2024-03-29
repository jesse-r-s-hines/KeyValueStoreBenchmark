#!/bin/bash
PROJ_ROOT="$(realpath $(dirname $(dirname "$0")))"
cd "$PROJ_ROOT"
set -e

if [ ! -d "vcpkg" ]; then
    git clone https://github.com/Microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh
else
    cd vcpkg
    git pull --ff-only
    cd ..
fi

# --overlay-ports="./vcpkg_overlay_ports" \
./vcpkg/vcpkg install \
    sqlite3 \
    rocksdb[snappy] \
    leveldb[snappy] \
    boost-process \
    boost-uuid \
    doctest \

if [ ! -d "build/berkeleydb" ]; then
    mkdir -p build
    cd build
    wget https://download.oracle.com/berkeley-db/db-18.1.40.zip
    unzip db-18.1.40.zip
    BERKELEYDB_SRC="$PROJ_ROOT/build/db-18.1.40"

    cd db-18.1.40/build_unix
    ../dist/configure --prefix="$PROJ_ROOT/build/berkeleydb/" --enable-cxx --enable-stl --with-repmgr-ssl=no
    make
    # workaround https://stackoverflow.com/questions/64707079/berkeley-db-make-install-fails-on-linux
    mkdir -p "$BERKELEYDB_SRC/docs/bdb-sql" "$BERKELEYDB_SRC/docs/gsg_db_server"
    make install

    cd "$PROJ_ROOT"
fi

# How to install BerkeleyDB in CMake
# if(NOT EXISTS ${CMAKE_BINARY_DIR}/berkeleydb/lib)
#     file(DOWNLOAD https://download.oracle.com/berkeley-db/db-18.1.40.zip ${CMAKE_BINARY_DIR}/berkeleydb.zip)
#     file(ARCHIVE_EXTRACT INPUT ${CMAKE_BINARY_DIR}/berkeleydb.zip)
#     set(BERKELEY_ROOT ${CMAKE_BINARY_DIR}/db-18.1.40)
#     execute_process(
#         COMMAND ../dist/configure --prefix=${CMAKE_BINARY_DIR}/berkeleydb/ --enable-cxx --enable-stl --with-repmgr-ssl=no
#         WORKING_DIRECTORY ${BERKELEY_ROOT}/build_unix
#         RESULT_VARIABLE ret
#     )
#     if (ret AND NOT ret EQUAL 0)
#         message(FATAL_ERROR "Bad exit status")
#     endif()

#     execute_process(COMMAND make WORKING_DIRECTORY ${BERKELEY_ROOT}/build_unix RESULT_VARIABLE ret)
#     if (ret AND NOT ret EQUAL 0)
#         message(FATAL_ERROR "Bad exit status")
#     endif()

#     # workaround https://stackoverflow.com/questions/64707079/berkeley-db-make-install-fails-on-linux
#     make_directory(${BERKELEY_ROOT}/docs/bdb-sql)
#     make_directory(${BERKELEY_ROOT}/docs/gsg_db_server)

#     execute_process(COMMAND make install WORKING_DIRECTORY ${BERKELEY_ROOT}/build_unix RESULT_VARIABLE ret)
#     if (ret AND NOT ret EQUAL 0)
#         message(FATAL_ERROR "Bad exit status")
#     endif()
# endif()

./scripts/downloadGutenberg.py
