#!/bin/bash

# 记录开始时间
START_TIME=$(date +%s.%N)

# 检查是否指定了测试模块
TEST_MODULE="$1"

# 设置测试文件列表
TEST_FILES="test_memory.c test_log.c test_sync.c test_error.c test_memory_pool.c test_net.c test_mux.c test_gc.c test_infra_config.c test_async.c"

# 如果没有指定测试模块，显示可用的测试模块
if [ -z "$TEST_MODULE" ]; then
    echo "Available test modules:"
    for f in $TEST_FILES; do
        echo "\t${f%.c}" | sed 's/test_//'
    done
    exit 0
fi

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 检查infra库是否需要重新构建
NEED_BUILD_INFRA=0
if [ ! -f "${BUILD_DIR}/infra/libinfra.a" ]; then
    NEED_BUILD_INFRA=1
else
    for f in "${SRC_DIR}"/internal/infra/*.c; do
        if [ "$f" -nt "${BUILD_DIR}/infra/libinfra.a" ]; then
            NEED_BUILD_INFRA=1
            break
        fi
    done
fi

echo ========= clean old begin
#find "${BUILD_DIR}/test/white/infra/"
rm -vrf "${BUILD_DIR}/test/white/infra/*"
echo ========= clean old end

if [ $NEED_BUILD_INFRA -eq 1 ]; then
    echo "Building infra library..."
    "$(dirname "$0")/build_infra.sh"
    if [ $? -ne 0 ]; then
        exit 1
    fi
else
    echo "Infra library is up to date."
fi

# 创建必要的目录
mkdir -p "${BUILD_DIR}/test/white/framework"

# 检查测试框架是否需要重新构建
NEED_BUILD_FRAMEWORK=0
if [ ! -f "${BUILD_DIR}/test/white/framework/test_framework.o" ]; then
    NEED_BUILD_FRAMEWORK=1
elif [ "${PPDB_DIR}/test/white/framework/test_framework.c" -nt "${BUILD_DIR}/test/white/framework/test_framework.o" ]; then
    NEED_BUILD_FRAMEWORK=1
fi

if [ $NEED_BUILD_FRAMEWORK -eq 1 ]; then
    echo "Building test framework..."
    "${CC}" ${CFLAGS} -I"${COSMOS}" -I"${PPDB_DIR}" -I"${PPDB_DIR}/include" -I"${SRC_DIR}" -I"${PPDB_DIR}/test/white/framework" "${PPDB_DIR}/test/white/framework/test_framework.c" -c -o "${BUILD_DIR}/test/white/framework/test_framework.o"
    if [ $? -ne 0 ]; then
        exit 1
    fi
else
    echo "Test framework is up to date."
fi

echo "Building test cases..."

# 创建必要的目录
mkdir -p "${BUILD_DIR}/test/white/infra"

# 如果指定了测试模块，则只构建该模块的测试
if [ ! -z "$TEST_MODULE" ]; then
    TEST_FILE="test_${TEST_MODULE}.c"
    if [ -f "${PPDB_DIR}/test/white/infra/${TEST_FILE}" ]; then
        TEST_FILES="${TEST_FILE}"
        echo "Building only ${TEST_MODULE} module tests..."
    else
        echo "Error: Test module '${TEST_MODULE}' not found"
        echo "Available modules:"
        for f in $TEST_FILES; do
            echo "    ${f%.c}" | sed 's/test_//'
        done
        exit 1
    fi
fi

# 编译和链接每个测试文件
for f in $TEST_FILES; do
    base="${f%.c}"
    NEED_BUILD=0
    if [ ! -f "${BUILD_DIR}/test/white/infra/${base}.o" ]; then
        NEED_BUILD=1
    elif [ "${PPDB_DIR}/test/white/infra/$f" -nt "${BUILD_DIR}/test/white/infra/${base}.o" ]; then
        NEED_BUILD=1
    fi
    
    if [ $NEED_BUILD -eq 1 ]; then
        echo "Building $f..."
        "${CC}" ${CFLAGS} \
            -I"${PPDB_DIR}" \
            -I"${PPDB_DIR}/include" \
            -I"${SRC_DIR}" \
            -I"${PPDB_DIR}/test" \
            -I"${PPDB_DIR}/test/white" \
            -I"${PPDB_DIR}/test/white/framework" \
            -I"${PPDB_DIR}/src/internal/infra" \
            "${PPDB_DIR}/test/white/infra/$f" \
            -c -o "${BUILD_DIR}/test/white/infra/${base}.o"
        if [ $? -ne 0 ]; then
            echo "Failed to build $f"
            exit 1
        fi
    else
        echo "$f is up to date."
    fi

    NEED_LINK=0
    if [ ! -f "${BUILD_DIR}/test/white/infra/${base}" ]; then
        NEED_LINK=1
    elif [ "${BUILD_DIR}/test/white/infra/${base}.o" -nt "${BUILD_DIR}/test/white/infra/${base}" ]; then
        NEED_LINK=1
    elif [ "${BUILD_DIR}/infra/libinfra.a" -nt "${BUILD_DIR}/test/white/infra/${base}" ]; then
        NEED_LINK=1
    fi
    
    if [ $NEED_LINK -eq 1 ]; then
        echo "Linking ${base}..."

        "${CC}" \
            "${BUILD_DIR}/test/white/framework/test_framework.o" \
            "${BUILD_DIR}/test/white/infra/${base}.o" \
            "${BUILD_DIR}/infra/libinfra.a" \
            ${LDFLAGS} ${LIBS} \
            -o "${BUILD_DIR}/test/white/infra/${base}.exe"
        if [ $? -ne 0 ]; then
            echo "Failed to link ${base}"
            exit 1
        fi

    else
        echo "${base}.exe binary is up to date."
    fi

    if [ "$2" != "norun" ]; then
        echo "Running ${base} tests..."
        "${BUILD_DIR}/test/white/infra/${base}.exe"
        echo
    fi
done

echo "Build complete."

# 计算耗时
END_TIME=$(date +%s.%N)
ELAPSED_TIME=$(echo "$END_TIME - $START_TIME" | bc)
printf "Total build time: %.3f seconds\n" $ELAPSED_TIME

exit 0
