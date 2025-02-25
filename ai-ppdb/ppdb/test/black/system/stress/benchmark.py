#!/usr/bin/env python3

"""
PPDB 性能基准测试
测试 PPDB 在不同场景下的性能表现
"""

import time
import threading
import random
import string
import argparse
import statistics
import json
from concurrent.futures import ThreadPoolExecutor
import requests

class PPDBBenchmark:
    def __init__(self, host="localhost", port=7000):
        """初始化基准测试"""
        self.base_url = f"http://{host}:{port}"
        self.latencies = []
        self.errors = 0
        self.total_ops = 0
    
    def random_string(self, length):
        """生成随机字符串"""
        return ''.join(random.choices(string.ascii_letters + string.digits, k=length))
    
    def put_operation(self, key, value):
        """执行 PUT 操作并记录延迟"""
        start_time = time.time()
        try:
            response = requests.put(
                f"{self.base_url}/kv/{key}",
                json={"value": value}
            )
            if response.status_code != 200:
                self.errors += 1
        except Exception as e:
            self.errors += 1
        finally:
            latency = time.time() - start_time
            self.latencies.append(latency)
            self.total_ops += 1
    
    def get_operation(self, key):
        """执行 GET 操作并记录延迟"""
        start_time = time.time()
        try:
            response = requests.get(f"{self.base_url}/kv/{key}")
            if response.status_code not in [200, 404]:
                self.errors += 1
        except Exception as e:
            self.errors += 1
        finally:
            latency = time.time() - start_time
            self.latencies.append(latency)
            self.total_ops += 1
    
    def run_benchmark(self, num_threads, num_ops, key_size=10, value_size=100):
        """运行基准测试"""
        print(f"Starting benchmark with {num_threads} threads, {num_ops} operations per thread")
        
        with ThreadPoolExecutor(max_workers=num_threads) as executor:
            for _ in range(num_threads):
                executor.submit(self._thread_operations, num_ops, key_size, value_size)
        
        self._print_results()
    
    def _thread_operations(self, num_ops, key_size, value_size):
        """单个线程的操作"""
        for i in range(num_ops):
            op = random.choice(['put', 'get'])
            key = self.random_string(key_size)
            
            if op == 'put':
                value = self.random_string(value_size)
                self.put_operation(key, value)
            else:
                self.get_operation(key)
    
    def _print_results(self):
        """输出测试结果"""
        if not self.latencies:
            print("No operations performed!")
            return
        
        # 计算统计数据
        avg_latency = statistics.mean(self.latencies)
        p95_latency = statistics.quantiles(self.latencies, n=20)[18]  # 95th percentile
        p99_latency = statistics.quantiles(self.latencies, n=100)[98]  # 99th percentile
        
        # 计算吞吐量
        total_time = sum(self.latencies)
        ops_per_sec = self.total_ops / total_time if total_time > 0 else 0
        
        results = {
            "total_operations": self.total_ops,
            "total_errors": self.errors,
            "average_latency_ms": avg_latency * 1000,
            "p95_latency_ms": p95_latency * 1000,
            "p99_latency_ms": p99_latency * 1000,
            "operations_per_second": ops_per_sec,
            "error_rate": (self.errors / self.total_ops) if self.total_ops > 0 else 0
        }
        
        print("\nBenchmark Results:")
        print(json.dumps(results, indent=2))

def main():
    parser = argparse.ArgumentParser(description='PPDB Benchmark Tool')
    parser.add_argument('--host', default='localhost', help='PPDB server host')
    parser.add_argument('--port', type=int, default=7000, help='PPDB server port')
    parser.add_argument('--threads', type=int, default=4, help='Number of threads')
    parser.add_argument('--ops', type=int, default=1000, help='Operations per thread')
    parser.add_argument('--key-size', type=int, default=10, help='Key size in bytes')
    parser.add_argument('--value-size', type=int, default=100, help='Value size in bytes')
    
    args = parser.parse_args()
    
    benchmark = PPDBBenchmark(args.host, args.port)
    benchmark.run_benchmark(args.threads, args.ops, args.key_size, args.value_size)

if __name__ == '__main__':
    main()
