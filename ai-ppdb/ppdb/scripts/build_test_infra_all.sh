#!/bin/bash

# 记录开始时间
START_TIME=$(date +%s.%N)

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

echo "Cleaning old build files..."
if [ -d "${BUILD_DIR}/test/white/infra" ]; then
    rm -f "${BUILD_DIR}/test/white/infra"/*.o
    rm -f "${BUILD_DIR}/test/white/infra"/*.exe
    rm -f "${BUILD_DIR}/test/white/infra"/*.dbg
fi

echo "Building all infra tests..."

MODULES="memory memory_pool error sync log net"
BUILD_FAILED=0

for module in $MODULES; do
    echo
    echo "========== Building ${module} module =========="
    "$(dirname "$0")/build_test_infra.sh" "${module}"
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to build ${module} module"
        BUILD_FAILED=1
        break
    fi
done

echo
echo "========== Build Summary =========="
if [ $BUILD_FAILED -eq 1 ]; then
    echo "Build FAILED"
else
    echo "Build completed successfully"
fi

# 计算耗时
END_TIME=$(date +%s.%N)
ELAPSED_TIME=$(echo "$END_TIME - $START_TIME" | bc)
printf "Total build time: %.3f seconds\n" $ELAPSED_TIME

exit $BUILD_FAILED 
