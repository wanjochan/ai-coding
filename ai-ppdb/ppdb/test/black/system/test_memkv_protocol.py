import unittest
import time
import logging
import socket
import pymemcache
from pymemcache.client.base import Client

# 配置日志
logging.basicConfig(
    level=logging.DEBUG,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)

class TestMemKVBasic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        logging.info("Connecting to memkv service on localhost:11211")
        cls.client = Client(('localhost', 11211), timeout=5.0)  # 添加5秒超时
        logging.info("Connection established")

    def setUp(self):
        logging.info("Setting up test - flushing all data")
        try:
            self.client.flush_all()
        except Exception as e:
            logging.error(f"Error in setUp: {e}")
            raise

    def test_basic_set_get(self):
        logging.info("Testing basic set/get...")
        key, value = 'key1', 'value1'
        result = self.client.set(key, value)
        self.assertTrue(result)
        got_value = self.client.get(key)
        self.assertEqual(got_value, b'value1')

    def test_delete(self):
        logging.info("Testing delete...")
        key, value = 'key1', 'value1'
        self.client.set(key, value)
        result = self.client.delete(key)
        self.assertTrue(result)
        try:
            got_value = self.client.get(key)
        except pymemcache.exceptions.MemcacheUnknownError as e:
            if b'NOT_FOUND' in str(e).encode():
                got_value = None
                logging.debug("Key not found as expected after deletion")
            else:
                logging.error(f"Unexpected error after deletion: {e}")
                raise
        self.assertIsNone(got_value)

    def test_not_found(self):
        logging.info("Testing non-existent keys...")
        key = 'nonexistent_key'
        try:
            value = self.client.get(key)
        except pymemcache.exceptions.MemcacheUnknownError as e:
            if b'NOT_FOUND' in str(e).encode():
                value = None
                logging.debug("Key not found as expected")
            else:
                logging.error(f"Unexpected error: {e}")
                raise
        self.assertIsNone(value)

class TestMemKVProtocol(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # 连接到已运行的服务
        logging.info("Connecting to memkv service on localhost:11211")
        cls.client = Client(('localhost', 11211))
        logging.info("Connection established")

    @classmethod
    def tearDownClass(cls):
        logging.info("Closing connection")
        try:
            cls.client.close()
        except Exception as e:
            logging.error(f"Error closing connection: {e}")

    def setUp(self):
        # 每个测试前清空数据
        logging.info("Setting up test - flushing all data")
        try:
            self.client.flush_all()
        except Exception as e:
            logging.error(f"Error in setUp: {e}")
            raise

    def test_multi_set_get(self):
        # 测试多个键值对
        logging.info("Testing multiple set/get...")
        test_data = {
            'key1': 'value1',
            'key2': 'value2',
            'key3': 'value3'
        }
        for k, v in test_data.items():
            logging.debug(f"Setting key='{k}' value='{v}'")
            result = self.client.set(k, v)
            logging.debug(f"Set result: {result}")
            self.assertTrue(result)
        
        for k, v in test_data.items():
            logging.debug(f"Getting key='{k}'")
            got_value = self.client.get(k)
            logging.debug(f"Got value: {got_value}")
            self.assertEqual(got_value, v.encode())

    def test_expiration(self):
        # 测试过期时间
        logging.info("Testing expiration...")
        key, value = 'key1', 'value1'
        
        logging.debug(f"Setting key='{key}' value='{value}' with 1s expiration")
        try:
            self.client.set(key, value, expire=1)
        except Exception as e:
            logging.error(f"Error setting key with expiration: {e}")
            raise
        
        logging.debug(f"Getting key immediately")
        try:
            got_value = self.client.get(key)
            logging.debug(f"Got value: {got_value}")
            self.assertEqual(got_value, b'value1')
        except Exception as e:
            logging.error(f"Error getting key immediately: {e}")
            raise
        
        logging.debug("Waiting for expiration (2s)")
        time.sleep(2)
        
        logging.debug(f"Getting key after expiration")
        try:
            got_value = self.client.get(key)
            logging.debug(f"Got value after expiration: {got_value}")
        except pymemcache.exceptions.MemcacheUnknownError as e:
            if b'NOT_FOUND' in str(e).encode():
                got_value = None
                logging.debug("Key not found as expected after expiration")
            else:
                logging.error(f"Unexpected error after expiration: {e}")
                raise
        except socket.timeout:
            logging.error("Socket timeout while getting expired key")
            raise
        except Exception as e:
            logging.error(f"Unexpected error type: {type(e)}, error: {e}")
            raise
        
        self.assertIsNone(got_value)

    def test_increment_decrement(self):
        # 测试自增自减
        logging.info("Testing increment/decrement...")
        key = 'counter'
        
        try:
            self.client.set(key, '0')
            self.assertEqual(self.client.incr(key, 1), 1)
            self.assertEqual(self.client.incr(key, 2), 3)
            self.assertEqual(self.client.decr(key, 1), 2)
        except Exception as e:
            logging.error(f"Error in increment/decrement test: {e}")
            raise

    def test_large_values(self):
        # 测试大数据
        logging.info("Testing large values...")
        key = 'large_key'
        large_value = 'x' * 1024 * 1024  # 1MB
        
        try:
            result = self.client.set(key, large_value)
            self.assertTrue(result)
            
            got_value = self.client.get(key)
            self.assertEqual(got_value, large_value.encode())
        except Exception as e:
            logging.error(f"Error in large values test: {e}")
            raise

if __name__ == '__main__':
    unittest.main(verbosity=2)
