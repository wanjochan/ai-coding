# PPDB 测试框架

## 目录结构
```
test/
```

## 快速开始

### 运行所有测试
```bash
# 在项目根目录下执行
./scripts/run_tests.sh
```

### 运行特定类型的测试
```bash
# 运行单元测试
./scripts/run_tests.sh unit

# 运行系统测试
./scripts/run_tests.sh system

# 运行性能测试
./scripts/run_tests.sh benchmark
```

## 测试类型说明

### 1. CLI 测试
- `test_basic_ops.sh`: 测试基本的 CRUD 操作
- `test_concurrent.sh`: 测试并发操作场景

### 2. HTTP API 测试
- `test_crud.py`: 测试 HTTP API 的 CRUD 操作
- `test_batch.py`: 测试批量操作
- `test_errors.py`: 测试错误处理

### 3. 压力测试
- `benchmark.py`: 性能基准测试工具
  ```bash
  python benchmark.py --threads 4 --ops 1000
  ```

### 4. 场景测试
- `basic_usage.py`: 测试基本使用场景
- `data_recovery.py`: 测试数据恢复场景

## 注意事项

1. 运行测试前确保：
   - PPDB 已经正确编译
   - 测试环境满足要求
   - 有足够的磁盘空间

2. 测试数据：
   - 测试会在 /tmp/ppdb_test 创建临时文件
   - 测试结束后会自动清理

3. 端口使用：
   - 测试默认使用 7000 端口
   - 确保端口未被占用

4. 并发测试：
   - 可能需要调整系统限制
   - 注意观察系统资源使用
