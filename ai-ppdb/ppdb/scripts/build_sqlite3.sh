#!/bin/bash

# 加载通用构建脚本
source "$(dirname "$0")/build_common.sh" || { echo "Error: Failed to load build_common.sh"; exit 1; }

# 创建构建目录
mkdir -p "${BUILD_DIR}/sqlite3"

# 构建 SQLite3
echo "Building SQLite3..."
SQLITE_LIB="${BUILD_DIR}/sqlite3/libsqlite3.a"
SQLITE_SRC="${PPDB_DIR}/vendor/sqlite3/sqlite3.c"

# 检查是否需要重新编译
if [ ! -f "${SQLITE_LIB}" ] || [ "${SQLITE_SRC}" -nt "${SQLITE_LIB}" ]; then
    # 编译
    echo "Compiling SQLite3..."
    "${CC}" ${CFLAGS} -c -o "${BUILD_DIR}/sqlite3/sqlite3.o" \
        "${SQLITE_SRC}"

    if [ $? -ne 0 ]; then
        echo "Error: Failed to compile sqlite3"
        exit 1
    fi

    # 创建静态库
    echo "Creating static library..."
    "${AR}" rcs "${SQLITE_LIB}" \
        "${BUILD_DIR}/sqlite3/sqlite3.o"

    if [ $? -ne 0 ]; then
        echo "Error: Failed to create sqlite3 static library"
        exit 1
    fi

    echo "SQLite3 build complete."
else
    echo "SQLite3 library is up to date, skipping build."
fi 
ls -al "${SQLITE_LIB}"