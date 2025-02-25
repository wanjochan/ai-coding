import sys

def read_log(filename):
    try:
        with open(filename, 'rb') as f:
            print(f"=== {filename} ===")
            content = f.read()
            for encoding in ['utf-8', 'gbk', 'ascii']:
                try:
                    text = content.decode(encoding)
                    print(f"[Using {encoding} encoding]")
                    print(text)
                    break
                except UnicodeDecodeError:
                    continue
            else:
                print("Failed to decode with any encoding")
                print("Raw hex dump:")
                print(' '.join(f'{b:02x}' for b in content))
    except Exception as e:
        print(f"Error reading {filename}: {e}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python read_log.py <logfile>")
        sys.exit(1)
    read_log(sys.argv[1]) 