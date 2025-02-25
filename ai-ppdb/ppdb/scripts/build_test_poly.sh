#!/bin/bash

# 记录开始时间
START_TIME=$(date +%s.%N)

# 设置颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 错误处理函数
handle_error() {
    local exit_code=$1
    local error_message=$2
    if [ $exit_code -ne 0 ]; then
        echo -e "${RED}Error: $error_message${NC}"
        exit $exit_code
    fi
}

# 检查是否需要重新构建
need_rebuild() {
    local target=$1
    shift
    local sources=("$@")
    
    # 如果目标文件不存在，需要构建
    if [ ! -f "$target" ]; then
        return 0
    fi
    
    # 检查每个源文件
    for src in "${sources[@]}"; do
        if [ ! -f "$src" ]; then
            echo -e "${RED}Error: Source file not found: $src${NC}"
            exit 1
        fi
        if [ "$src" -nt "$target" ]; then
            return 0
        fi
    done
    
    return 1
}

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
handle_error $? "Failed to load build environment"

# 检查 sqlite3.h 和 duckdb.h 是否存在
if [ ! -f "${PPDB_DIR}/vendor/sqlite3/sqlite3.h" ]; then
    echo -e "${RED}Error: sqlite3.h not found in ${PPDB_DIR}/vendor/sqlite3/${NC}"
    exit 1
fi

if [ ! -f "${PPDB_DIR}/vendor/duckdb/duckdb.h" ]; then
    echo -e "${RED}Error: duckdb.h not found in ${PPDB_DIR}/vendor/duckdb/${NC}" 
    exit 1
fi

#echo clean up ${BUILD_DIR}
#rm -rvf ${BUILD_DIR}/*

# 创建构建目录
echo -e "${GREEN}Creating build directories...${NC}"
mkdir -p "${BUILD_DIR}/test/black/poly"
handle_error $? "Failed to create test/black/poly directory"

mkdir -p "${BUILD_DIR}/vendor/sqlite3"
handle_error $? "Failed to create vendor/sqlite3 directory"

mkdir -p "${BUILD_DIR}/vendor/duckdb"
handle_error $? "Failed to create vendor/duckdb directory"

# 设置测试二进制文件路径
TEST_DB_BIN="${BUILD_DIR}/test/black/poly/test_poly_db"

# 编译 SQLite
SQLITE_OBJ="${BUILD_DIR}/vendor/sqlite3/sqlite3.o"
SQLITE_SRC="${PPDB_DIR}/vendor/sqlite3/sqlite3.c"
if need_rebuild "$SQLITE_OBJ" "$SQLITE_SRC"; then
    echo -e "${GREEN}Building SQLite...${NC}"
    ${CC} ${CFLAGS} \
        -I"${PPDB_DIR}/vendor/sqlite3" \
        "$SQLITE_SRC" \
        -c -o "$SQLITE_OBJ"
    handle_error $? "Failed to build SQLite"
else
    echo -e "${YELLOW}SQLite is up to date, skipping build${NC}"
fi

# 复制 DuckDB 动态库 (TODO 不需要，到时是从 exe所在的当前目录加载就好）
DUCKDB_LIB="${BUILD_DIR}/vendor/duckdb/libduckdb.dylib"
DUCKDB_SRC="${PPDB_DIR}/vendor/duckdb/libduckdb.dylib"
if need_rebuild "$DUCKDB_LIB" "$DUCKDB_SRC"; then
    echo -e "${GREEN}Copying DuckDB library...${NC}"
    cp "$DUCKDB_SRC" "$DUCKDB_LIB"
    handle_error $? "Failed to copy DuckDB library"
else
    echo -e "${YELLOW}DuckDB library is up to date, skipping copy${NC}"
fi

# 编译测试源文件
echo -e "${GREEN}Building poly tests...${NC}"

# 清理旧的测试文件
rm -f "${BUILD_DIR}/test/black/poly/test_poly_sqlitekv"
rm -f "${BUILD_DIR}/test/black/poly/test_poly_duckdbkv"
rm -f "${BUILD_DIR}/test/black/poly/test_poly_memkv"

# 不需要，测死和产品的 o 分开吧
# echo "sh build_poly.sh"
# sh "$(dirname "$0")/build_poly.sh"



MEMKV_IMPL_OBJ="${BUILD_DIR}/test/black/poly/poly_memkv.o"
MEMKV_IMPL_SRC="${PPDB_DIR}/src/internal/poly/poly_memkv.c"
if need_rebuild "$MEMKV_IMPL_OBJ" "$MEMKV_IMPL_SRC"; then
    echo -e "${GREEN}Building MemKV implementation...${NC}"
    ${CC} ${CFLAGS} \
        -I"${PPDB_DIR}" \
        -I"${PPDB_DIR}/include" \
        -I"${PPDB_DIR}/vendor/sqlite3" \
        -I"${PPDB_DIR}/vendor/duckdb" \
        -I"${PPDB_DIR}/src" \
        "$MEMKV_IMPL_SRC" \
        -c -o "$MEMKV_IMPL_OBJ"
    handle_error $? "Failed to build MemKV implementation"
else
    echo -e "${YELLOW}MemKV implementation is up to date, skipping build${NC}"
fi

# 编译实现文件
ATOMIC_OBJ="${BUILD_DIR}/test/black/poly/poly_atomic.o"
ATOMIC_SRC="${PPDB_DIR}/src/internal/poly/poly_atomic.c"
if need_rebuild "$ATOMIC_OBJ" "$ATOMIC_SRC"; then
    echo -e "${GREEN}Building atomic implementation...${NC}"
    ${CC} ${CFLAGS} \
        -I"${PPDB_DIR}" \
        -I"${PPDB_DIR}/include" \
        -I"${PPDB_DIR}/src" \
        "$ATOMIC_SRC" \
        -c -o "$ATOMIC_OBJ"
    handle_error $? "Failed to build atomic implementation"
else
    echo -e "${YELLOW}Atomic implementation is up to date, skipping build${NC}"
fi

PLUGIN_OBJ="${BUILD_DIR}/test/black/poly/poly_plugin.o"
PLUGIN_SRC="${PPDB_DIR}/src/internal/poly/poly_plugin.c"
if need_rebuild "$PLUGIN_OBJ" "$PLUGIN_SRC"; then
    echo -e "${GREEN}Building plugin implementation...${NC}"
    ${CC} ${CFLAGS} \
        -I"${PPDB_DIR}" \
        -I"${PPDB_DIR}/include" \
        -I"${PPDB_DIR}/src" \
        "$PLUGIN_SRC" \
        -c -o "$PLUGIN_OBJ"
    handle_error $? "Failed to build plugin implementation"
else
    echo -e "${YELLOW}Plugin implementation is up to date, skipping build${NC}"
fi

# 编译测试框架
TEST_FRAMEWORK_OBJ="${BUILD_DIR}/test/black/poly/test_framework.o"
TEST_FRAMEWORK_SRC="${PPDB_DIR}/test/white/framework/test_framework.c"
if need_rebuild "$TEST_FRAMEWORK_OBJ" "$TEST_FRAMEWORK_SRC"; then
    echo -e "${GREEN}Building test framework...${NC}"
    ${CC} ${CFLAGS} \
        -I"${PPDB_DIR}" \
        -I"${PPDB_DIR}/include" \
        -I"${PPDB_DIR}/src" \
        "$TEST_FRAMEWORK_SRC" \
        -c -o "$TEST_FRAMEWORK_OBJ"
    handle_error $? "Failed to build test framework"
else
    echo -e "${YELLOW}Test framework is up to date, skipping build${NC}"
fi

# 编译 poly_db
echo -e "${GREEN}Building poly_db...${NC}"
mkdir -p "${BUILD_DIR}/test/black/poly"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    -I"${PPDB_DIR}/vendor/sqlite3" \
    -I"${PPDB_DIR}/vendor/duckdb" \
    -c "${PPDB_DIR}/src/internal/poly/poly_db.c" \
    -o "${BUILD_DIR}/test/black/poly/poly_db.o"
handle_error $? "Failed to compile poly_db"

# 编译 poly_db 测试
echo -e "${GREEN}Building poly_db test...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    -I"${PPDB_DIR}/vendor/sqlite3" \
    -I"${PPDB_DIR}/vendor/duckdb" \
    -c "${PPDB_DIR}/test/poly/test_poly_db.c" \
    -o "${BUILD_DIR}/test/poly/test_poly_db.o"
handle_error $? "Failed to compile poly_db test"

# 编译 infra_memory
echo -e "${GREEN}Building infra_memory...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    -c "${PPDB_DIR}/src/internal/infra/infra_memory.c" \
    -o "${BUILD_DIR}/test/black/poly/infra_memory.o"
handle_error $? "Failed to compile infra_memory"

# 编译 infra_sync
echo -e "${GREEN}Building infra_sync...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    -c "${PPDB_DIR}/src/internal/infra/infra_sync.c" \
    -o "${BUILD_DIR}/test/black/poly/infra_sync.o"
handle_error $? "Failed to compile infra_sync"

# 编译 infra_platform
echo -e "${GREEN}Building infra_platform...${NC}"
${CC} ${CFLAGS} \
    -I"${PPDB_DIR}" \
    -I"${PPDB_DIR}/include" \
    -I"${PPDB_DIR}/src" \
    -c "${PPDB_DIR}/src/internal/infra/infra_platform.c" \
    -o "${BUILD_DIR}/test/black/poly/infra_platform.o"
handle_error $? "Failed to compile infra_platform"

# 链接 poly_db 测试
echo -e "${GREEN}Linking poly_db test...${NC}"
${CC} ${CFLAGS} \
    -o "${TEST_DB_BIN}" \
    "${BUILD_DIR}/test/poly/test_poly_db.o" \
    "${BUILD_DIR}/test/black/poly/poly_db.o" \
    "${BUILD_DIR}/test/black/poly/test_framework.o" \
    "${BUILD_DIR}/test/black/poly/infra_memory.o" \
    "${BUILD_DIR}/test/black/poly/infra_sync.o" \
    "${BUILD_DIR}/test/black/poly/infra_platform.o" \
    "${BUILD_DIR}/infra/libinfra.a" \
    "${BUILD_DIR}/vendor/sqlite3/sqlite3.o" \
    -ldl -lpthread -lm \
    ${LDFLAGS}
handle_error $? "Failed to link poly_db test"

# 运行测试
echo -e "${GREEN}Running poly_db tests...${NC}"
"$TEST_DB_BIN"
handle_error $? "poly_db tests failed"

# 计算并显示总耗时
END_TIME=$(date +%s.%N)
DURATION=$(echo "$END_TIME - $START_TIME" | bc)
echo -e "${GREEN}All tests completed successfully in ${DURATION} seconds.${NC}"

exit 0 
