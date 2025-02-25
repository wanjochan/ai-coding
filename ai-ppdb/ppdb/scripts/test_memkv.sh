#!/bin/bash
set -e  # Exit on error

#!/bin/bash

# 导入公共函数和环境变量
source "$(dirname "$0")/build_env.sh"
source "$(dirname "$0")/build_common.sh"

# 检查工作目录变量
if [ ! -d "${ROOT_DIR}" ]; then
    echo "Error: ROOT_DIR is not a valid directory: ${ROOT_DIR}"
    exit 1
fi

# 检查PPDB_DIR
if [ ! -d "${PPDB_DIR}" ]; then
    echo "Error: PPDB_DIR is not a valid directory: ${PPDB_DIR}"
    exit 1
fi

# 可配置的参数
TIMEOUT_SERVER=30  # 服务器超时时间
TIMEOUT_CLIENT=20  # 客户端超时时间
LOG_LEVEL=5       # 日志级别
SERVER_PORT=11211 # 服务器端口

# 可选的测试用例参数
#test_case="-k test_multi_set_get"
test_case=$* # 测试用例名称
echo test_case: ${test_case}

# 创建临时日志目录
LOG_DIR="${PPDB_DIR}/logs"
mkdir -p "${LOG_DIR}"
# 从test_case中提取测试用例名称
if [[ $test_case =~ -k[[:space:]]*([^[:space:]]+) ]]; then
    test_case_name="${BASH_REMATCH[1]}"
    echo "Extracted test case name: ${test_case_name}"
else
    test_case_name=""
    echo "No specific test case specified, will run all tests"
fi

SERVER_LOG="${LOG_DIR}/memkv_server_${test_case_name}.log"
CLIENT_LOG="${LOG_DIR}/memkv_client_${test_case_name}.log"

# 确保在正确的目录中
cd "${PPDB_DIR}"

# 构建项目
echo "Building project..."
if ! sh "${ROOT_DIR}/ppdb/scripts/build_ppdb.sh"; then
    echo "Error: Build failed"
    exit 1
fi

# 确保没有遗留的服务进程
echo "Cleaning up old processes..."
pkill -9 -f "ppdb_latest.exe memkv" || true
sleep 1

# 启动服务器
echo "Starting memkv server..."
(timeout ${TIMEOUT_SERVER}s "${PPDB_DIR}/ppdb_latest.exe" memkv --start --config="${PPDB_DIR}/memkv.conf" --log-level=${LOG_LEVEL} 2>&1 | tee "${SERVER_LOG}" &)

# 等待服务器启动
echo "Waiting for server to start..."
for i in {1..10}; do
    if nc -z localhost ${SERVER_PORT}; then
        break
    fi
    sleep 1
    if [ $i -eq 10 ]; then
        echo "Error: Server failed to start"
        pkill -9 -f "ppdb_latest.exe memkv"
        exit 1
    fi
done

# 运行测试
echo "Running tests..."
if ! (timeout ${TIMEOUT_CLIENT}s ${HOME}/miniconda3/bin/python3 "${PPDB_DIR}/test/black/system/test_memkv_protocol.py" ${test_case} 2>&1 | tee "${CLIENT_LOG}"); then
    echo "Warning: Tests failed or timed out"
fi

# 清理
echo "Cleaning up..."
pkill -9 -f "ppdb_latest.exe memkv" || true

# 显示测试结果
echo "Test completed. Logs available at:"

echo =============server log: ${SERVER_LOG} start=============
cat ${SERVER_LOG}
echo =============server log: ${SERVER_LOG} end=============
echo =============client log: ${CLIENT_LOG} start=============
cat ${CLIENT_LOG}
echo =============client log: ${CLIENT_LOG} end=============
