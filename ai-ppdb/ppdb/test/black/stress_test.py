#!/usr/bin/env python3

"""
PPDB Stress Test Script
This script performs stress testing on a PPDB server using the memcached protocol.
"""

import memcache
import random
import string
import time
import threading
import argparse
import sys
import logging
from concurrent.futures import ThreadPoolExecutor

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(threadName)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class PPDBStressTest:
    def __init__(self, host='127.0.0.1', port=11211, num_threads=4, duration=60):
        self.host = host
        self.port = port
        self.num_threads = num_threads
        self.duration = duration
        self.client = None
        self.stats = {
            'set_ops': 0,
            'get_ops': 0,
            'delete_ops': 0,
            'set_errors': 0,
            'get_errors': 0,
            'delete_errors': 0
        }
        self.stats_lock = threading.Lock()
        self.running = False

    def connect(self):
        """Initialize memcached client connection"""
        try:
            server = f"{self.host}:{self.port}"
            self.client = memcache.Client([server], debug=True)
            # Test connection
            test_key = 'test_connection'
            test_value = 'test_value'
            if not self.client.set(test_key, test_value):
                raise Exception("Failed to set test value")
            if self.client.get(test_key) != test_value:
                raise Exception("Failed to get test value")
            self.client.delete(test_key)
            logger.info(f"Connected to {server}")
            return True
        except Exception as e:
            logger.error(f"Client initialization failed: {e}")
            return False

    def generate_random_string(self, length=10):
        """Generate a random string of fixed length"""
        return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

    def update_stats(self, operation, success):
        """Update operation statistics"""
        with self.stats_lock:
            if success:
                self.stats[f'{operation}_ops'] += 1
            else:
                self.stats[f'{operation}_errors'] += 1

    def worker(self):
        """Worker function that performs operations"""
        while self.running:
            key = self.generate_random_string()
            value = self.generate_random_string(100)
            
            # SET operation
            try:
                success = self.client.set(key, value)
                self.update_stats('set', success)
            except Exception as e:
                logger.error(f"SET operation failed: {e}")
                self.update_stats('set', False)

            # GET operation
            try:
                result = self.client.get(key)
                self.update_stats('get', result == value)
            except Exception as e:
                logger.error(f"GET operation failed: {e}")
                self.update_stats('get', False)

            # DELETE operation
            try:
                success = self.client.delete(key)
                self.update_stats('delete', success)
            except Exception as e:
                logger.error(f"DELETE operation failed: {e}")
                self.update_stats('delete', False)

    def run_test(self):
        """Run the stress test"""
        if not self.connect():
            logger.error("Failed to initialize test client")
            return False

        logger.info(f"Starting stress test with {self.num_threads} threads for {self.duration} seconds")
        self.running = True
        
        with ThreadPoolExecutor(max_workers=self.num_threads) as executor:
            futures = [executor.submit(self.worker) for _ in range(self.num_threads)]
            
            # Wait for the specified duration
            time.sleep(self.duration)
            
            # Stop the test
            self.running = False
            
            # Wait for all workers to complete
            for future in futures:
                future.result()

        # Log results
        total_ops = sum(v for k, v in self.stats.items() if k.endswith('_ops'))
        total_errors = sum(v for k, v in self.stats.items() if k.endswith('_errors'))
        
        if total_ops > 0:
            logger.info("Stress test completed successfully")
            logger.info(f"Total operations: {total_ops}")
            logger.info(f"Total errors: {total_errors}")
            logger.info(f"Success rate: {(total_ops / (total_ops + total_errors)) * 100:.2f}%")
            logger.info(f"Operations per second: {total_ops / self.duration:.2f}")
            return True
        else:
            logger.error("Stress test failed - no successful operations")
            return False

def main():
    parser = argparse.ArgumentParser(description='PPDB Stress Test')
    parser.add_argument('--host', default='127.0.0.1', help='Server host')
    parser.add_argument('--port', type=int, default=11211, help='Server port')
    parser.add_argument('--threads', type=int, default=4, help='Number of worker threads')
    parser.add_argument('--duration', type=int, default=60, help='Test duration in seconds')
    
    args = parser.parse_args()
    
    test = PPDBStressTest(
        host=args.host,
        port=args.port,
        num_threads=args.threads,
        duration=args.duration
    )
    
    success = test.run_test()
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main() 