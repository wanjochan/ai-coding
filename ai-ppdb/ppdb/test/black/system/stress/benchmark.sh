#!/bin/bash

# PPDB 性能基准测试脚本
# 用途：测试 PPDB 在各种场景下的性能表现

# 配置
PPDB_BIN="../../build/ppdb"
PPDB_CLI="../../build/ppdb-cli"
TEST_PORT=7000
TEST_DIR="/tmp/ppdb_benchmark"
DATA_SIZES=(16 256 4096 65536)  # 测试不同数据大小（字节）
NUM_OPERATIONS=10000            # 每轮测试的操作数

# 测试初始化
init_test() {
    echo "Initializing benchmark environment..."
    rm -rf "$TEST_DIR"
    mkdir -p "$TEST_DIR"
}

# 启动服务器
start_server() {
    echo "Starting PPDB server..."
    $PPDB_BIN --port $TEST_PORT --data-dir "$TEST_DIR" &
    PPDB_PID=$!
    sleep 2  # 等待服务器启动
}

# 停止服务器
stop_server() {
    echo "Stopping PPDB server..."
    kill $PPDB_PID
    wait $PPDB_PID 2>/dev/null
    rm -rf "$TEST_DIR"
}

# 生成随机数据
generate_data() {
    local size=$1
    head -c $size /dev/urandom | base64 | tr -d '\n'
}

# 写性能测试
benchmark_write() {
    local size=$1
    local count=$NUM_OPERATIONS
    local data=$(generate_data $size)
    local start_time=$(date +%s.%N)
    
    echo "Running write benchmark with ${size}B values..."
    for i in $(seq 1 $count); do
        $PPDB_CLI -p $TEST_PORT put "bench_key_$i" "$data" > /dev/null
    done
    
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    local ops_per_sec=$(echo "$count / $duration" | bc)
    
    echo "Write Performance (${size}B):"
    echo "  Total operations: $count"
    echo "  Duration: $duration seconds"
    echo "  Operations/sec: $ops_per_sec"
    echo "  Data written: $(echo "$count * $size / 1024 / 1024" | bc) MB"
}

# 读性能测试
benchmark_read() {
    local size=$1
    local count=$NUM_OPERATIONS
    local data=$(generate_data $size)
    
    # 先写入数据
    echo "Preparing data for read benchmark..."
    for i in $(seq 1 $count); do
        $PPDB_CLI -p $TEST_PORT put "bench_key_$i" "$data" > /dev/null
    done
    
    # 开始读取测试
    echo "Running read benchmark with ${size}B values..."
    local start_time=$(date +%s.%N)
    
    for i in $(seq 1 $count); do
        $PPDB_CLI -p $TEST_PORT get "bench_key_$i" > /dev/null
    done
    
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    local ops_per_sec=$(echo "$count / $duration" | bc)
    
    echo "Read Performance (${size}B):"
    echo "  Total operations: $count"
    echo "  Duration: $duration seconds"
    echo "  Operations/sec: $ops_per_sec"
    echo "  Data read: $(echo "$count * $size / 1024 / 1024" | bc) MB"
}

# 混合读写测试
benchmark_mixed() {
    local size=$1
    local count=$NUM_OPERATIONS
    local data=$(generate_data $size)
    
    echo "Running mixed read/write benchmark with ${size}B values..."
    local start_time=$(date +%s.%N)
    
    for i in $(seq 1 $count); do
        if [ $((RANDOM % 2)) -eq 0 ]; then
            $PPDB_CLI -p $TEST_PORT put "bench_key_$i" "$data" > /dev/null
        else
            $PPDB_CLI -p $TEST_PORT get "bench_key_$i" > /dev/null
        fi
    done
    
    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    local ops_per_sec=$(echo "$count / $duration" | bc)
    
    echo "Mixed Performance (${size}B):"
    echo "  Total operations: $count"
    echo "  Duration: $duration seconds"
    echo "  Operations/sec: $ops_per_sec"
}

# 延迟测试
benchmark_latency() {
    local size=$1
    local count=1000  # 使用较少的操作以获得更准确的延迟数据
    local data=$(generate_data $size)
    local total_write_latency=0
    local total_read_latency=0
    
    echo "Running latency benchmark with ${size}B values..."
    
    # 写入延迟测试
    for i in $(seq 1 $count); do
        local start_time=$(date +%s.%N)
        $PPDB_CLI -p $TEST_PORT put "latency_key_$i" "$data" > /dev/null
        local end_time=$(date +%s.%N)
        local latency=$(echo "($end_time - $start_time) * 1000" | bc)  # 转换为毫秒
        total_write_latency=$(echo "$total_write_latency + $latency" | bc)
    done
    
    # 读取延迟测试
    for i in $(seq 1 $count); do
        local start_time=$(date +%s.%N)
        $PPDB_CLI -p $TEST_PORT get "latency_key_$i" > /dev/null
        local end_time=$(date +%s.%N)
        local latency=$(echo "($end_time - $start_time) * 1000" | bc)  # 转换为毫秒
        total_read_latency=$(echo "$total_read_latency + $latency" | bc)
    done
    
    local avg_write_latency=$(echo "$total_write_latency / $count" | bc -l)
    local avg_read_latency=$(echo "$total_read_latency / $count" | bc -l)
    
    echo "Latency Results (${size}B):"
    echo "  Average Write Latency: ${avg_write_latency}ms"
    echo "  Average Read Latency: ${avg_read_latency}ms"
}

# 主测试流程
main() {
    init_test
    start_server
    
    echo "=== PPDB Performance Benchmark ==="
    echo "Starting benchmark with ${NUM_OPERATIONS} operations per test..."
    
    for size in "${DATA_SIZES[@]}"; do
        echo
        echo "=== Testing with ${size}B values ==="
        benchmark_write $size
        benchmark_read $size
        benchmark_mixed $size
        benchmark_latency $size
    done
    
    stop_server
    echo "Benchmark completed!"
}

# 运行测试
main
