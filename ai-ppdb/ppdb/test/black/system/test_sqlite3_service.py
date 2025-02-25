import unittest
import socket
import threading
import time
import logging

# 配置日志
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

class SQLite3Client:
    def __init__(self, host='localhost', port=15433):
        self.host = host
        self.port = port
        self.sock = None
    
    def connect(self):
        logging.info(f"Connecting to {self.host}:{self.port}")
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        self.sock.settimeout(1)  # 设置较短的超时时间，但会多次重试
        logging.info("Connection established")
    
    def close(self):
        if self.sock:
            logging.info("Closing connection")
            self.sock.close()
            self.sock = None
    
    def execute(self, sql):
        if not self.sock:
            raise Exception("Not connected")
        
        logging.debug(f"Executing SQL: {sql}")
        # 确保 SQL 命令以换行符结尾
        if not sql.endswith('\n'):
            sql += '\n'
        self.sock.sendall(sql.encode())
        
        # 读取响应
        response = b''
        start_time = time.time()
        max_wait_time = 5  # 最大等待时间（秒）
        
        while True:
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                response += chunk
                
                # 检查是否收到完整响应（以换行符结尾）
                if response.endswith(b'\n'):
                    break
                
            except socket.timeout:
                # 检查是否超过最大等待时间
                if time.time() - start_time > max_wait_time:
                    logging.warning("Timeout waiting for response")
                    break
                continue  # 继续尝试接收
            
            except Exception as e:
                logging.error(f"Error receiving response: {e}")
                break
        
        if not response:
            return None
        
        # 检查错误
        response_str = response.decode().strip()
        if response_str.startswith("ERROR:"):
            raise Exception(f"SQL execution failed: {response_str}")
        
        return response_str

class TestSQLite3Service(unittest.TestCase):
    def setUp(self):
        self.client = SQLite3Client()
        self.client.connect()
    
    def tearDown(self):
        self.client.close()
    
    def test_basic_operations(self):
        logging.info("Testing basic operations...")
        
        # 创建表
        logging.debug("Creating test table")
        self.client.execute('''
            CREATE TABLE test (
                id INTEGER PRIMARY KEY,
                name TEXT,
                value REAL
            )
        ''')
        
        # 插入数据
        logging.debug("Inserting test data")
        self.client.execute('INSERT INTO test VALUES (1, "test1", 1.1)')
        self.client.execute('INSERT INTO test VALUES (2, "test2", 2.2)')
        
        # 查询数据
        logging.debug("Querying test data")
        result = self.client.execute('SELECT * FROM test')
        logging.debug(f"Query result: {result}")
        
        # 更新数据
        logging.debug("Updating test data")
        self.client.execute('UPDATE test SET value = 3.3 WHERE id = 1')
        
        # 删除数据
        logging.debug("Deleting test data")
        self.client.execute('DELETE FROM test WHERE id = 2')

    def test_transaction(self):
        logging.info("Testing transaction support...")
        
        # 创建表
        logging.debug("Creating transaction test table")
        self.client.execute('CREATE TABLE trans_test (id INTEGER PRIMARY KEY, value TEXT)')
        
        try:
            # 开始事务
            logging.debug("Starting transaction")
            self.client.execute('BEGIN TRANSACTION')
            self.client.execute('INSERT INTO trans_test VALUES (1, "value1")')
            self.client.execute('INSERT INTO trans_test VALUES (2, "value2")')
            # 故意制造错误
            logging.debug("Intentionally causing a primary key conflict")
            self.client.execute('INSERT INTO trans_test VALUES (2, "value3")')  # 主键冲突
        except Exception as e:
            logging.debug(f"Expected error occurred: {e}")
            self.client.execute('ROLLBACK')

    def test_concurrent_connections(self):
        logging.info("Testing concurrent connections...")
        def worker(worker_id):
            logging.debug(f"Worker {worker_id} starting")
            client = SQLite3Client()
            try:
                client.connect()
                client.execute('CREATE TABLE IF NOT EXISTS concurrent_test (id INTEGER PRIMARY KEY, worker_id INTEGER)')
                client.execute(f'INSERT INTO concurrent_test VALUES ({worker_id}, {worker_id})')
                logging.debug(f"Worker {worker_id} completed successfully")
            except Exception as e:
                logging.error(f"Worker {worker_id} failed: {e}")
                raise
            finally:
                client.close()
        
        # 创建多个线程并发访问
        threads = []
        for i in range(10):
            t = threading.Thread(target=worker, args=(i,))
            threads.append(t)
            t.start()
        
        # 等待所有线程完成
        for t in threads:
            t.join()
        
        # 验证结果
        logging.debug("Verifying concurrent test results")
        result = self.client.execute('SELECT COUNT(*) FROM concurrent_test')
        logging.debug(f"Total concurrent records: {result}")

if __name__ == '__main__':
    unittest.main() 