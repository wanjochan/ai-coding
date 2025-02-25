#-*- coding: utf-8 -*-
"""
全自动任务流转器

功能：
- 任务的创建、更新、查询等操作（结合 sqlite 数据库）
- 命令行接口（网页接口见 taskmon.py 和 taskmon.html）
- 适合各类 IDE+LLM，唯一要求是他们能执行命令行

命令行用法例子：
python ai/tasker.py check --role Dev
python ai/tasker.py check --role PM
python ai/tasker.py check --role User
python ai/tasker.py send --from-role User --to-role PM --subject "Test Task" --content "This is a test task"
python ai/tasker.py update-status --role PM --task-id <task_id> --status processing
python ai/tasker.py recall --from-role PM --task-id <task_id>
"""

import sys
import os
import json
import argparse
import hashlib
from datetime import datetime
from typing import Optional, Dict, Any, List
from enum import Enum

# 添加项目根目录到 Python 路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from ai.taskdb import DBManager
from ai.role_prompts import RolePrompts

class TaskStatus(str, Enum):
    """任务状态枚举"""
    UNREAD = "unread"
    PROCESSING = "processing"
    REPLIED = "replied" 
    COMPLETED = "completed"
    ARCHIVED = "archived"
    RECALLED = "recalled"

# 添加状态码常量
class StatusCode:
    """状态码常量"""
    OK = "0"  # 成功
    NO_TASK = "1"  # 无任务
    ERROR = "2"  # 错误
    HAS_CHANGES = "3"  # 有变更
    NO_CHANGES = "4"  # 无变更

class TaskManager:
    def __init__(self):
        self.db = DBManager()
        self.roles = ["User", "PM", "Dev", "DS"]  # 添加DS角色

    def _generate_task_id(self, content: str) -> str:
        """生成任务ID"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        content_hash = hashlib.md5(content.encode()).hexdigest()[:8]
        return f"{timestamp}_{content_hash}"

    def send_task(self, from_role: str, to_role: str, subject: str,
                 content: str, reply_to: str = None) -> str:
        """发送任务"""
        if from_role not in self.roles or to_role not in self.roles:
            raise ValueError(f"Invalid role: {from_role} or {to_role}")

        task_id = self._generate_task_id(content)
        self.db.create_task(from_role, to_role, subject, content, task_id, reply_to)
        return task_id

    def recall_task(self, from_role: str, task_id: str) -> None:
        """撤回任务（仅限未读任务）"""
        tasks = self.db.get_tasks_by_role(from_role, include_sent=True)
        task = next((m for m in tasks if m['id'] == task_id), None)
        
        if not task:
            raise ValueError(f"Task not found: {task_id}")
        
        if task['from_role'] != from_role:
            raise ValueError("Can only recall tasks you sent")
        
        if task.get('read_at'):
            raise ValueError("Cannot recall task that has been read")
        
        self.db.update_task_status(task_id, TaskStatus.RECALLED.value,
                                 "Task recalled by sender")

    def get_tasks_by_status(self, role: str, status: Optional[TaskStatus] = None,
                           include_sent: bool = False, include_replied: bool = False) -> List[Dict[str, Any]]:
        """获取指定状态的任务（优化版）
        Args:
            role: 角色名称
            status: 可选的任务状态过滤
            include_sent: 是否包含发送的任务
            include_replied: 是否包含已回复的任务（默认不包含）
        """
        if role not in self.roles:
            raise ValueError(f"Invalid role: {role}")

        excluded_statuses = [TaskStatus.COMPLETED.value, TaskStatus.ARCHIVED.value]
        if not include_replied:
            excluded_statuses.append(TaskStatus.REPLIED.value)
            
        return self.db.get_tasks_by_role(
            role,
            status=status.value if status else None,
            include_sent=include_sent,
            excluded_statuses=excluded_statuses
        )

    def update_task_status(self, role: str, task_id: str,
                          new_status: TaskStatus, note: str = None) -> None:
        """更新任务状态"""
        tasks = self.db.get_tasks_by_role(role, include_sent=True)
        task = next((m for m in tasks if m['id'] == task_id), None)
        
        if not task:
            raise ValueError(f"Task not found or no permission: {task_id}")
        
        self.db.update_task_status(task_id, new_status.value, note)

def format_task_output(task: Dict[str, Any]) -> str:
    """格式化任务输出（简化版）"""
    return f"{task['id']}|{task['from_role']}|{task['to_role']}|{task['status']}|{task['subject']}"

def format_task_detail(task: Dict[str, Any]) -> str:
    """格式化任务详细信息"""
    return (f"ID: {task['id']}\n"
            f"From: {task['from_role']}\n"
            f"To: {task['to_role']}\n"
            f"Subject: {task['subject']}\n"
            f"Status: {task['status']}\n"
            f"Content: {task['content']}\n"
            f"Created: {task['created_at']}")

def output_result(status_code: str, data: Any = None, message: str = "") -> None:
    """输出结果（简化格式）"""
    if status_code == StatusCode.OK:
        print(f"{status_code}:{data if data is not None else ''}")
    elif status_code == StatusCode.ERROR:
        print(f"{status_code}:{message}")
    elif status_code == StatusCode.HAS_CHANGES:
        print(f"{status_code}:{data if data is not None else 0}")
    elif status_code == StatusCode.NO_CHANGES:
        print(f"{status_code}:0")
    else:
        print(f"{status_code}:")

def check_tasks(task_manager: TaskManager, role: str, status: Optional[str] = None,
                include_sent: bool = False, show_detail: bool = False,
                include_replied: bool = False) -> None:
    """检查任务（优化版）"""
    status_enum = TaskStatus(status) if status else None
    try:
        tasks = task_manager.get_tasks_by_status(role, status_enum, include_sent,
                                               include_replied)
        
        if not tasks:
            output_result(StatusCode.NO_TASK)
            return

        # 根据是否需要详细信息选择输出格式
        if show_detail:
            task_list = [format_task_detail(task) for task in tasks]
        else:
            task_list = [format_task_output(task) for task in tasks]
            
        output_result(StatusCode.OK, "\n".join(task_list))

    except ValueError as e:
        output_result(StatusCode.ERROR, message=str(e))

def main():
    parser = argparse.ArgumentParser(description='Task Manager')
    
    # 添加子命令
    subparsers = parser.add_subparsers(dest='mode', help='Operation mode')
    
    # 检查任务命令
    check_parser = subparsers.add_parser('check', help='Check tasks')
    check_parser.add_argument('--role', required=True, help='Role to check tasks for')
    check_parser.add_argument('--status', choices=[s.value for s in TaskStatus],
                           help='Task status to filter')
    check_parser.add_argument('--include-sent', action='store_true',
                           help='Include sent tasks')
    check_parser.add_argument('--detail', action='store_true',
                           help='Show detailed task information')
    check_parser.add_argument('--include-replied', action='store_true',
                           help='Include replied tasks')
    
    # 发送任务命令
    send_parser = subparsers.add_parser('send', help='Send task')
    send_parser.add_argument('--from-role', required=True, help='Sender role')
    send_parser.add_argument('--to-role', required=True, help='Recipient role')
    send_parser.add_argument('--subject', required=True, help='Task subject')
    send_parser.add_argument('--content', required=True, help='Task content')
    send_parser.add_argument('--reply-to', help='Original task ID')
    
    # 更新状态命令
    update_parser = subparsers.add_parser('update-status', help='Update task status')
    update_parser.add_argument('--role', required=True, help='Role updating the task')
    update_parser.add_argument('--task-id', required=True, help='Task ID')
    update_parser.add_argument('--status', required=True,
                            choices=[s.value for s in TaskStatus],
                            help='New status')
    update_parser.add_argument('--note', help='Status update note')
    
    # 撤回任务命令
    recall_parser = subparsers.add_parser('recall', help='Recall task')
    recall_parser.add_argument('--from-role', required=True, help='Role recalling the task')
    recall_parser.add_argument('--task-id', required=True, help='Task ID')
    
    # 检查变更命令
    changes_parser = subparsers.add_parser('check-changes', help='Check for changes')
    changes_parser.add_argument('--last-id', type=int, default=0,
                             help='Last change ID')
    
    # 删除已完成任务命令
    delete_parser = subparsers.add_parser('delete-completed',
                                       help='Delete completed tasks')
    
    # 查看角色提示词命令
    prompt_parser = subparsers.add_parser('prompt', help='Show role prompt')
    prompt_parser.add_argument('--role', required=True, help='Role to show prompt for')
    
    args = parser.parse_args()
    task_manager = TaskManager()

    try:
        if args.mode == 'prompt':
            # 显示角色提示词
            prompt = RolePrompts.get_prompt(args.role)
            if prompt:
                print(prompt)
                return
            else:
                print(f"No prompt found for role: {args.role}")
                return
            
        elif args.mode == 'check':
            check_tasks(task_manager, args.role, args.status,
                       args.include_sent, args.detail, args.include_replied)
        
        elif args.mode == 'check-changes':
            with task_manager.db.get_connection() as conn:
                cursor = conn.cursor()
                cursor.execute("""
                    SELECT id FROM changes 
                    WHERE id > ? 
                    ORDER BY id DESC 
                    LIMIT 1
                """, (args.last_id,))
                row = cursor.fetchone()
                
                if row:
                    output_result(StatusCode.HAS_CHANGES, row[0])
                else:
                    output_result(StatusCode.NO_CHANGES)
        
        elif args.mode == 'send':
            task_id = task_manager.send_task(args.from_role, args.to_role,
                                           args.subject, args.content, args.reply_to)
            output_result(StatusCode.OK, task_id)
        
        elif args.mode == 'update-status':
            task_manager.update_task_status(args.role, args.task_id,
                                          TaskStatus(args.status), args.note)
            output_result(StatusCode.OK)

        elif args.mode == 'recall':
            task_manager.recall_task(args.from_role, args.task_id)
            output_result(StatusCode.OK)
            
        elif args.mode == 'delete-completed':
            deleted_count = task_manager.db.delete_completed_tasks()
            output_result(StatusCode.OK, deleted_count)
        
        elif args.mode == 'prompt':
            # 显示角色提示词
            prompt = RolePrompts.get_prompt(args.role)
            if prompt:
                print(prompt)
                return
            else:
                print(f"No prompt found for role: {args.role}")
                return
        
        else:
            parser.print_help()
            sys.exit(1)

    except Exception as e:
        output_result(StatusCode.ERROR, message=str(e))
        sys.exit(1)

if __name__ == '__main__':
    main()
'''notes

#codebase
Roo，请你阅读 tasker.py 了解它的命令行用法。然后：

1）按顺序分别以 PM、Dev 角色去用命令行查收任务：
   # 先检查任务列表
   python ai/tasker.py check --role PM
   python ai/tasker.py check --role Dev
   
   # 如果有任务，获取详细信息
   python ai/tasker.py check --role PM --detail
   python ai/tasker.py check --role Dev --detail

2）如果有新任务就进行分析、计划、执行、回复、更新状态；
3）如果本轮全部角色都没有新任务就sleep 15 秒
4）回到第 1 步

Roo，请你阅读 tasker.py 了解它的命令行用法。然后：

1）按顺序分别以 DS 角色去用命令行查收任务：
   # 先检查任务列表
   python ai/tasker.py check --role DS
   
   # 如果有任务，获取详细信息
   python ai/tasker.py check --role DS --detail

2）如果有新任务就进行分析、计划、执行、回复、更新状态；
3）如果本轮全部角色都没有新任务就sleep 15 秒
4）回到第 1 步
'''
