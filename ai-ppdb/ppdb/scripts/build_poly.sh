#!/bin/bash

# 见 docs/ARCH.md

# 加载通用构建脚本
source "$(dirname "$0")/build_common.sh" || { echo "Error: Failed to load build_common.sh"; exit 1; }

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}/poly"

# 定义源文件
SRC_FILES=(
    "${SRC_DIR}/internal/poly/poly_db.c"
    "${SRC_DIR}/internal/poly/poly_cmdline.c"
    "${SRC_DIR}/internal/poly/poly_atomic.c"
    "${SRC_DIR}/internal/poly/poly_plugin.c"
    "${SRC_DIR}/internal/poly/poly_poll.c"
    # TODO: 等 memkv 模块完成后再启用
    # "${SRC_DIR}/internal/poly/poly_async.c"
)

# 设置编译标志
CFLAGS="${CFLAGS} -I${PPDB_DIR}/src -I${PPDB_DIR}/vendor/sqlite3 -I${PPDB_DIR}/vendor/duckdb"

# 编译所有源文件
for src in "${SRC_FILES[@]}"; do
    obj="${BUILD_DIR}/poly/$(basename "${src}" .c).o"
    echo "-e Compiling ${src}..."
    "${CC}" ${CFLAGS} -I"${PPDB_DIR}/include" -I"${SRC_DIR}" -c "${src}" -o "${obj}"
    if [ $? -ne 0 ]; then
        echo "-e Error: Failed to compile ${src}"
        exit 1
    fi
done

# 创建静态库
echo "-e Creating static library..."
cd "${BUILD_DIR}/poly" || exit 1
"${AR}" rcs "libpoly.a" *.o
if [ $? -ne 0 ]; then
    echo "-e Error: Failed to create static library"
    exit 1
fi

# 确保静态库已创建
if [ ! -f "libpoly.a" ]; then
    echo "-e Error: Static library not created"
    exit 1
fi

echo "-e Build complete."
ls -l "libpoly.a"
echo "-e Build completed in $SECONDS seconds."
