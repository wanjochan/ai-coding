#!/usr/bin/env python3
"""Enhanced File Line Replacement Tool

A specialized tool designed for AI agents to modify files, with robust handling of
edge cases and errors. Particularly optimized for Windows/PowerShell environments.

Key Features:
1. Safe file operations with automatic backup
2. Robust handling of line numbers and content
3. Support for both single and batch operations
4. Automatic handling of encodings and line endings
5. Detailed error reporting

Usage for AI Agents:

1. Basic line replacement:
    @'
    1,2:new content
    '@ | python filechanger.py file.txt

2. Multiple operations:
    @(
        '1,1:# File header',
        '5,6:def new_function():\\n    print("Hello")',
        '-1,0:# End of file'
    ) | python filechanger.py file.txt

Common Operations:
- Replace lines:     '5,6:new content'
- Delete lines:      '3,4:'
- Insert at start:   '0,0:first line'
- Append at end:     '-1,0:last line'
- Replace one line:  '5,5:new line'

Line Number Rules:
- Numbers start from 1
- Negative indices count from end (-1 = last line)
- 0,0 means insert before first line
- -1,0 means append after last line
- Invalid numbers are automatically adjusted to valid range

Special Features:
- Creates missing files and directories
- Handles UTF-8 and system encodings
- Creates .bak files on write failures
- Normalizes line endings
- Supports \\n for newlines in content

Error Handling:
- Detailed error messages with line numbers
- Safe handling of invalid inputs
- Graceful handling of encoding issues
- Automatic backup on write failures
"""

import sys
import os
import io

def ensure_file_exists(filename):
    """确保文件存在，如果不存在则创建空文件"""
    if not os.path.exists(filename):
        os.makedirs(os.path.dirname(os.path.abspath(filename)), exist_ok=True)
        with open(filename, 'w', encoding='utf-8') as f:
            pass

def normalize_line_endings(content):
    """标准化换行符"""
    if not content:
        return content
    # 将所有换行符转换为 \n
    content = content.replace('\r\n', '\n').replace('\r', '\n')
    # 确保内容以换行符结尾
    if not content.endswith('\n'):
        content += '\n'
    return content

def replace_lines(filename, start, end, content=""):
    """替换文件中的指定行范围为新内容
    
    Args:
        filename: 要处理的文件路径
        start: 起始行号(支持负数,如 -1 表示最后一行)
        end: 结束行号(支持负数)
        content: 新的内容(默认为空字符串,表示删除这些行)
        
    Raises:
        OSError: 如果文件操作失败
        ValueError: 如果行号格式无效
    """
    # 确保文件存在
    ensure_file_exists(filename)
    
    try:
        # 确保以 UTF-8 编码读取文件
        with open(filename, 'r', encoding='utf-8') as f:
            lines = f.readlines()
    except UnicodeDecodeError:
        # 如果 UTF-8 解码失败，尝试系统默认编码
        with open(filename, 'r') as f:
            lines = f.readlines()
    
    total = len(lines)
    
    # 处理负数行号
    start = start if start > 0 else total + start + 1
    end = end if end > 0 else total + end + 1
    
    # 确保行号在有效范围内
    start = max(1, min(start, total + 1))
    end = max(start, min(end, total + 1))
    
    # 特殊情况：插入到文件开头
    if start == 0 and end == 0:
        start = end = 1
    
    # 特殊情况：追加到文件末尾
    if start == total + 1:
        lines.append('')
    
    # 处理内容
    if isinstance(content, str):
        content = normalize_line_endings(content)
        content = content.splitlines(True)
    
    # 替换指定范围的行
    lines[start-1:end] = content
    
    # 确保文件以换行符结尾
    if lines and not lines[-1].endswith('\n'):
        lines[-1] += '\n'
    
    # 以 UTF-8 编码写回文件
    try:
        with open(filename, 'w', encoding='utf-8', newline='') as f:
            f.writelines(lines)
    except Exception as e:
        # 如果写入失败，尝试创建备份
        backup_file = filename + '.bak'
        with open(backup_file, 'w', encoding='utf-8', newline='') as f:
            f.writelines(lines)
        raise OSError(f"Failed to write to {filename}. Backup saved to {backup_file}. Error: {str(e)}")

def process_batch_input(filename, batch_content):
    """处理批量替换输入
    
    Args:
        filename: 要处理的文件路径
        batch_content: 批处理内容字符串
        
    Raises:
        ValueError: 如果输入格式无效
    """
    # 确保文件存在
    ensure_file_exists(filename)
    
    # 处理每一行批处理命令
    for line_num, cmd in enumerate(batch_content.splitlines(), 1):
        cmd = cmd.strip()
        if not cmd:
            continue
            
        try:
            # 分割行号和内容
            if ':' not in cmd:
                continue
            
            range_part, content = cmd.split(':', 1)
            if ',' not in range_part:
                raise ValueError(f"Invalid range format in line {line_num}: {range_part}")
                
            try:
                start, end = map(str.strip, range_part.split(','))
                start = int(start)
                end = int(end)
            except ValueError:
                raise ValueError(f"Invalid line numbers in line {line_num}: {range_part}")
                
            # 处理转义序列
            try:
                content = content.encode('raw_unicode_escape').decode('unicode_escape')
            except UnicodeDecodeError:
                # 如果解码失败，使用原始内容
                pass
            
            # 执行替换
            replace_lines(filename, start, end, content)
            
        except Exception as e:
            print(f"Error processing line {line_num} '{cmd}': {str(e)}", file=sys.stderr)
            raise

def test_replace_lines():
    """测试行替换功能"""
    test_file = "test_filechanger.txt"
    
    # 测试用例
    test_cases = [
        # 基本替换
        (1, 2, "new line1\nnew line2", "Basic replacement"),
        # 删除行
        (3, 4, "", "Delete lines"),
        # 在开头插入
        (0, 0, "new first line", "Insert at beginning"),
        # 在末尾追加
        (-1, 0, "new last line", "Append at end"),
        # 超出范围的行号
        (10, 11, "should be appended", "Out of range line numbers"),
        # 负数行号
        (-2, -1, "new last lines\nreally last", "Negative line numbers"),
        # 特殊字符
        (1, 1, "line with \\n and \\t", "Special characters"),
        # Unicode 字符
        (2, 2, "Unicode: \u4f60\u597d\u4e16\u754c", "Unicode characters"),
    ]
    
    try:
        # 运行测试
        for start, end, content, desc in test_cases:
            print(f"\nTest: {desc}", file=sys.stderr)
            try:
                replace_lines(test_file, start, end, content)
                with open(test_file, "r", encoding='utf-8') as f:
                    print(f.read(), file=sys.stderr)
            except Exception as e:
                print(f"Error: {str(e)}", file=sys.stderr)
        
        # 测试批处理
        print("\nTest: Batch processing", file=sys.stderr)
        batch_content = """1,2:header line 1\nheader line 2
3,4:middle\ncontent
-1,0:last line 1\nlast line 2"""
        process_batch_input(test_file, batch_content)
        with open(test_file, "r", encoding='utf-8') as f:
            print(f.read(), file=sys.stderr)
            
    finally:
        # 清理测试文件
        if os.path.exists(test_file):
            os.remove(test_file)

if __name__ == '__main__':
    # 显示帮助
    if len(sys.argv) < 2 or sys.argv[1] in ['--help', '-h']:
        print(__doc__, file=sys.stderr)
        sys.exit(1)
    
    # 运行测试
    if sys.argv[1] == '--test':
        print("Running tests...", file=sys.stderr)
        test_replace_lines()
        sys.exit(0)
    
    # 执行替换
    try:
        filename = sys.argv[1]
        
        # 检查是否有管道输入
        if not sys.stdin.isatty():
            # 从管道读取内容
            if hasattr(sys.stdin, 'buffer'):
                content = io.TextIOWrapper(sys.stdin.buffer, encoding='utf-8').read()
            else:
                content = sys.stdin.read()
            
            # 根据内容格式选择处理方式
            if ':' in content:
                process_batch_input(filename, content)
            else:
                if len(sys.argv) < 4:
                    raise ValueError("Single replacement mode requires <start> and <end> parameters")
                start = int(sys.argv[2])
                end = int(sys.argv[3])
                replace_lines(filename, start, end, content)
        else:
            # 命令行模式
            if len(sys.argv) < 4:
                raise ValueError("Command line mode requires <start> and <end> parameters")
            start = int(sys.argv[2])
            end = int(sys.argv[3])
            content = sys.argv[4] if len(sys.argv) > 4 else ""
            replace_lines(filename, start, end, content)
            
    except Exception as e:
        print(f"Error: {str(e)}", file=sys.stderr)
        sys.exit(1)
