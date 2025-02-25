"""
任务数据库管理模块
"""

import os
import sqlite3
from datetime import datetime
from contextlib import contextmanager
from typing import Optional, List, Dict, Any, Union

class DBManager:
    def __init__(self, db_path: str = "tasks.db"):
        self.db_path = db_path
        self._init_db()

    def _init_db(self) -> None:
        """初始化数据库"""
        with self.get_connection() as conn:
            cursor = conn.cursor()
            
            # 创建任务表
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS tasks (
                    id TEXT PRIMARY KEY,
                    from_role TEXT NOT NULL,
                    to_role TEXT NOT NULL,
                    subject TEXT NOT NULL,
                    content TEXT NOT NULL,
                    status TEXT NOT NULL,
                    created_at TEXT NOT NULL,
                    read_at TEXT,
                    recalled_at TEXT,
                    reply_to TEXT,
                    last_active_at TEXT
                )
            """)
            
            # 创建变更记录表
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS changes (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    task_id TEXT NOT NULL,
                    change_type TEXT NOT NULL,
                    change_time TEXT NOT NULL,
                    details TEXT,
                    FOREIGN KEY (task_id) REFERENCES tasks (id)
                )
            """)
            
            # 创建会话表
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS sessions (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    current_role TEXT NOT NULL,
                    current_task_id TEXT,
                    ide_pid INTEGER,
                    started_at TEXT NOT NULL,
                    last_active_at TEXT NOT NULL,
                    status TEXT NOT NULL,
                    FOREIGN KEY (current_task_id) REFERENCES tasks (id)
                )
            """)
            
            # 创建会话历史表
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS session_history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    session_id INTEGER NOT NULL,
                    role TEXT NOT NULL,
                    task_id TEXT NOT NULL,
                    action TEXT NOT NULL,
                    timestamp TEXT NOT NULL,
                    FOREIGN KEY (session_id) REFERENCES sessions (id),
                    FOREIGN KEY (task_id) REFERENCES tasks (id)
                )
            """)
            
            conn.commit()

    @contextmanager
    def get_connection(self):
        """获取数据库连接"""
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        try:
            yield conn
        finally:
            conn.close()

    def create_session(self, role: str) -> int:
        """创建新会话"""
        now = datetime.now()
        with self.get_connection() as conn:
            cursor = conn.execute(
                """
                INSERT INTO sessions (current_role, started_at, last_active_at, status)
                VALUES (?, ?, ?, 'ACTIVE')
                """,
                (role, now, now)
            )
            conn.commit()
            return cursor.lastrowid

    def update_session(self, session_id: int, role: str = None, task_id: str = None,
                      ide_pid: int = None, status: str = None) -> None:
        """更新会话状态"""
        updates = []
        params = []
        if role is not None:
            updates.append("current_role = ?")
            params.append(role)
        if task_id is not None:
            updates.append("current_task_id = ?")
            params.append(task_id)
        if ide_pid is not None:
            updates.append("ide_pid = ?")
            params.append(ide_pid)
        if status is not None:
            updates.append("status = ?")
            params.append(status)
        
        updates.append("last_active_at = ?")
        params.append(datetime.now())
        params.append(session_id)

        with self.get_connection() as conn:
            conn.execute(
                f"""
                UPDATE sessions
                SET {', '.join(updates)}
                WHERE id = ?
                """,
                params
            )
            conn.commit()

    def add_session_history(self, session_id: int, role: str, task_id: str, action: str) -> None:
        """添加会话历史记录"""
        with self.get_connection() as conn:
            conn.execute(
                """
                INSERT INTO session_history (session_id, role, task_id, action, timestamp)
                VALUES (?, ?, ?, ?, ?)
                """,
                (session_id, role, task_id, action, datetime.now())
            )
            conn.commit()

    def get_active_session(self) -> Optional[Dict[str, Any]]:
        """获取活动会话"""
        with self.get_connection() as conn:
            cursor = conn.execute(
                """
                SELECT * FROM sessions
                WHERE status = 'ACTIVE'
                ORDER BY last_active_at DESC
                LIMIT 1
                """
            )
            row = cursor.fetchone()
            return dict(row) if row else None

    def create_task(self, from_role: str, to_role: str, subject: str, content: str,
                   task_id: str, reply_to: Optional[str] = None) -> None:
        """创建新任务"""
        now = datetime.now().isoformat()
        with self.get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("""
                INSERT INTO tasks (
                    id, from_role, to_role, subject, content,
                    status, created_at, reply_to, last_active_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (task_id, from_role, to_role, subject, content,
                  "unread", now, reply_to, now))
            
            # 记录变更
            cursor.execute("""
                INSERT INTO changes (task_id, change_type, change_time, details)
                VALUES (?, ?, ?, ?)
            """, (task_id, "create", now, f"Task created by {from_role}"))
            
            conn.commit()

    def update_task_status(self, task_id: str, new_status: str,
                          note: Optional[str] = None) -> None:
        """更新任务状态"""
        now = datetime.now().isoformat()
        with self.get_connection() as conn:
            cursor = conn.cursor()
            
            # 更新任务状态
            updates = ["status = ?", "last_active_at = ?"]
            params = [new_status, now]
            
            if new_status == "recalled":
                updates.append("recalled_at = ?")
                params.append(now)
            elif new_status not in ("unread", "recalled"):
                updates.append("read_at = COALESCE(read_at, ?)")
                params.append(now)
            
            params.append(task_id)
            
            cursor.execute(f"""
                UPDATE tasks
                SET {", ".join(updates)}
                WHERE id = ?
            """, params)
            
            # 记录变更
            cursor.execute("""
                INSERT INTO changes (task_id, change_type, change_time, details)
                VALUES (?, ?, ?, ?)
            """, (task_id, "status_update", now,
                  f"Status updated to {new_status}" + (f": {note}" if note else "")))
            
            conn.commit()

    def get_task_status(self, task_id: str) -> Optional[str]:
        """获取任务状态"""
        with self.get_connection() as conn:
            cursor = conn.execute(
                "SELECT status FROM tasks WHERE id = ?",
                (task_id,)
            )
            row = cursor.fetchone()
            return row['status'] if row else None

    def get_tasks_by_role(self, role: str, status: Optional[str] = None,
                         include_sent: bool = False,
                         excluded_statuses: Optional[List[str]] = None) -> List[Dict[str, Any]]:
        """获取指定角色的任务"""
        with self.get_connection() as conn:
            cursor = conn.cursor()
            
            # 构建查询条件
            conditions = []
            params = []
            
            if include_sent:
                conditions.append("(to_role = ? OR from_role = ?)")
                params.extend([role, role])
            else:
                conditions.append("to_role = ?")
                params.append(role)
            
            if status:
                conditions.append("status = ?")
                params.append(status)
            elif excluded_statuses:
                placeholders = ",".join("?" * len(excluded_statuses))
                conditions.append(f"status NOT IN ({placeholders})")
                params.extend(excluded_statuses)
            
            # 组合查询语句
            query = "SELECT * FROM tasks"
            if conditions:
                query += " WHERE " + " AND ".join(conditions)
            query += " ORDER BY last_active_at DESC"
            
            cursor.execute(query, params)
            columns = [col[0] for col in cursor.description]
            return [dict(zip(columns, row)) for row in cursor.fetchall()]

    def get_task_history(self, task_id: str) -> List[Dict[str, Any]]:
        """获取任务状态历史"""
        with self.get_connection() as conn:
            cursor = conn.execute(
                """
                SELECT * FROM task_status_history
                WHERE task_id = ?
                ORDER BY timestamp
                """,
                (task_id,)
            )
            return [dict(row) for row in cursor.fetchall()]

    def delete_completed_tasks(self) -> int:
        """删除已完成的任务"""
        with self.get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("DELETE FROM tasks WHERE status IN ('completed', 'archived')")
            deleted_count = cursor.rowcount
            conn.commit()
            return deleted_count