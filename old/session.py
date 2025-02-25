import os
import signal
import psutil
from datetime import datetime
from typing import Optional, Dict, Any
from .db import DBManager

class SessionManager:
    def __init__(self):
        self.db = DBManager()
        self._current_session = None
        self._load_active_session()

    def _load_active_session(self) -> None:
        """加载活动会话"""
        session = self.db.get_active_session()
        if session:
            # 检查IDE进程是否还在运行
            if session['ide_pid'] and not self._is_process_running(session['ide_pid']):
                session['ide_pid'] = None
                self.db.update_session(session['id'], ide_pid=None)
            self._current_session = session

    def _is_process_running(self, pid: int) -> bool:
        """检查进程是否在运行"""
        try:
            process = psutil.Process(pid)
            return process.is_running()
        except psutil.NoSuchProcess:
            return False

    def get_current_session(self) -> Optional[Dict[str, Any]]:
        """获取当前会话"""
        return self._current_session

    def start_session(self, role: str) -> Dict[str, Any]:
        """开始新会话"""
        if self._current_session:
            raise RuntimeError("已有活动会话")
        
        session_id = self.db.create_session(role)
        self._current_session = {
            'id': session_id,
            'current_role': role,
            'current_mail_id': None,
            'ide_pid': None,
            'started_at': datetime.now(),
            'last_active_at': datetime.now(),
            'status': 'ACTIVE'
        }
        return self._current_session

    def switch_role(self, new_role: str) -> None:
        """切换角色"""
        if not self._current_session:
            raise RuntimeError("没有活动会话")
        
        self.db.update_session(self._current_session['id'], role=new_role)
        self._current_session['current_role'] = new_role
        self._current_session['last_active_at'] = datetime.now()

    def set_current_mail(self, mail_id: str) -> None:
        """设置当前处理的邮件"""
        if not self._current_session:
            raise RuntimeError("没有活动会话")
        
        self.db.update_session(self._current_session['id'], mail_id=mail_id)
        self._current_session['current_mail_id'] = mail_id
        self._current_session['last_active_at'] = datetime.now()

    def set_ide_pid(self, pid: int) -> None:
        """设置IDE进程ID"""
        if not self._current_session:
            raise RuntimeError("没有活动会话")
        
        self.db.update_session(self._current_session['id'], ide_pid=pid)
        self._current_session['ide_pid'] = pid
        self._current_session['last_active_at'] = datetime.now()

    def end_session(self) -> None:
        """结束会话"""
        if not self._current_session:
            return
        
        # 尝试关闭IDE进程
        if self._current_session['ide_pid']:
            try:
                os.kill(self._current_session['ide_pid'], signal.SIGTERM)
            except ProcessLookupError:
                pass
        
        self.db.update_session(self._current_session['id'], status='COMPLETED')
        self._current_session = None

    def pause_session(self) -> None:
        """暂停会话"""
        if not self._current_session:
            return
        
        self.db.update_session(self._current_session['id'], status='PAUSED')
        self._current_session['status'] = 'PAUSED'

    def resume_session(self) -> None:
        """恢复会话"""
        if not self._current_session:
            return
        
        self.db.update_session(self._current_session['id'], status='ACTIVE')
        self._current_session['status'] = 'ACTIVE'

    def add_history(self, action: str) -> None:
        """添加会话历史"""
        if not self._current_session or not self._current_session['current_mail_id']:
            return
        
        self.db.add_session_history(
            self._current_session['id'],
            self._current_session['current_role'],
            self._current_session['current_mail_id'],
            action
        ) 