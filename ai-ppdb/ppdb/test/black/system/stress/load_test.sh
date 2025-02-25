#!/bin/bash

# PPDB 负载测试脚本
# 用途：测试 PPDB 在高负载和长期运行情况下的稳定性

# 配置
PPDB_BIN="../../build/ppdb"
PPDB_CLI="../../build/ppdb-cli"
TEST_PORT=7000
TEST_DIR="/tmp/ppdb_load_test"
NUM_CLIENTS=10           # 并发客户端数
TEST_DURATION=3600      # 测试持续时间（秒）
MONITOR_INTERVAL=60     # 监控间隔（秒）

# 测试初始化
init_test() {
    echo "Initializing load test environment..."
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
    head -c 1024 /dev/urandom | base64 | tr -d '\n'
}

# 监控系统资源
monitor_resources() {
    local pid=$1
    local output_file="resource_usage.log"
    
    echo "Time,CPU%,Memory(KB),DiskRead(KB),DiskWrite(KB)" > "$output_file"
    
    while kill -0 $pid 2>/dev/null; do
        local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
        local cpu=$(ps -p $pid -o %cpu | tail -n 1)
        local memory=$(ps -p $pid -o rss | tail -n 1)
        local disk_stats=$(awk '/sda/ {print $6,$10}' /proc/diskstats)
        
        echo "$timestamp,$cpu,$memory,$disk_stats" >> "$output_file"
        sleep $MONITOR_INTERVAL
    done
}

# 单个客户端的工作负载
client_workload() {
    local client_id=$1
    local start_time=$2
    local operations=0
    
    while true; do
        current_time=$(date +%s)
        if [ $((current_time - start_time)) -ge $TEST_DURATION ]; then
            break
        fi
        
        # 随机选择操作（70% 读，30% 写）
        if [ $((RANDOM % 100)) -lt 70 ]; then
            # 读操作
            $PPDB_CLI -p $TEST_PORT get "key_${client_id}_$operations" > /dev/null
        else
            # 写操作
            local data=$(generate_data)
            $PPDB_CLI -p $TEST_PORT put "key_${client_id}_$operations" "$data" > /dev/null
        fi
        
        operations=$((operations + 1))
        
        # 随机休眠 0-100ms，模拟真实负载
        sleep 0.$(printf "%03d" $((RANDOM % 100)))
    done
    
    echo "Client $client_id completed $operations operations"
}

# 启动负载测试
run_load_test() {
    local start_time=$(date +%s)
    
    echo "Starting load test with $NUM_CLIENTS clients for $TEST_DURATION seconds..."
    
    # 启动资源监控
    monitor_resources $PPDB_PID &
    MONITOR_PID=$!
    
    # 启动客户端
    for i in $(seq 1 $NUM_CLIENTS); do
        client_workload $i $start_time &
        CLIENT_PIDS[$i]=$!
    done
    
    # 等待所有客户端完成
    for pid in "${CLIENT_PIDS[@]}"; do
        wait $pid
    done
    
    # 停止监控
    kill $MONITOR_PID
    wait $MONITOR_PID 2>/dev/null
}

# 分析测试结果
analyze_results() {
    echo "Analyzing test results..."
    
    # 计算总操作数
    local total_ops=0
    for log in client_*.log; do
        if [ -f "$log" ]; then
            ops=$(tail -n 1 "$log" | awk '{print $NF}')
            total_ops=$((total_ops + ops))
        fi
    done
    
    # 分析资源使用情况
    if [ -f "resource_usage.log" ]; then
        echo "Resource Usage Summary:"
        echo "  Peak CPU Usage: $(sort -t',' -k2 -nr resource_usage.log | head -n 1 | cut -d',' -f2)%"
        echo "  Peak Memory Usage: $(sort -t',' -k3 -nr resource_usage.log | head -n 1 | cut -d',' -f3) KB"
        echo "  Average CPU Usage: $(awk -F',' '{sum+=$2} END {print sum/NR}' resource_usage.log)%"
        echo "  Average Memory Usage: $(awk -F',' '{sum+=$3} END {print sum/NR}' resource_usage.log) KB"
    fi
    
    echo "Performance Summary:"
    echo "  Total Operations: $total_ops"
    echo "  Average Operations/sec: $(echo "$total_ops / $TEST_DURATION" | bc)"
    echo "  Test Duration: $TEST_DURATION seconds"
}

# 检查内存泄漏
check_memory_leaks() {
    echo "Checking for memory leaks..."
    if command -v valgrind >/dev/null 2>&1; then
        valgrind --leak-check=full \
                --show-leak-kinds=all \
                --track-origins=yes \
                --log-file=valgrind_report.log \
                $PPDB_BIN --port $((TEST_PORT + 1)) --data-dir "${TEST_DIR}_valgrind" &
        VALGRIND_PID=$!
        
        sleep 5
        
        # 执行一些基本操作
        for i in {1..100}; do
            $PPDB_CLI -p $((TEST_PORT + 1)) put "leak_test_key_$i" "value_$i"
            $PPDB_CLI -p $((TEST_PORT + 1)) get "leak_test_key_$i"
        done
        
        kill $VALGRIND_PID
        wait $VALGRIND_PID 2>/dev/null
        
        echo "Memory leak report generated in valgrind_report.log"
    else
        echo "Valgrind not found, skipping memory leak check"
    fi
}

# 主测试流程
main() {
    init_test
    start_server
    
    echo "=== PPDB Load Test ==="
    run_load_test
    analyze_results
    check_memory_leaks
    
    stop_server
    echo "Load test completed!"
}

# 运行测试
main
