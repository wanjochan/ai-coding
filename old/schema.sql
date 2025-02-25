-- 任务表
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
);

-- 变更记录表
CREATE TABLE IF NOT EXISTS changes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id TEXT NOT NULL,
    change_type TEXT NOT NULL,
    change_time TEXT NOT NULL,
    details TEXT,
    FOREIGN KEY (task_id) REFERENCES tasks (id)
);

-- 会话表
CREATE TABLE IF NOT EXISTS sessions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    current_role TEXT NOT NULL,
    current_task_id TEXT,
    ide_pid INTEGER,
    started_at TEXT NOT NULL,
    last_active_at TEXT NOT NULL,
    status TEXT NOT NULL,
    FOREIGN KEY (current_task_id) REFERENCES tasks (id)
);

-- 会话历史表
CREATE TABLE IF NOT EXISTS session_history (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id INTEGER NOT NULL,
    role TEXT NOT NULL,
    task_id TEXT NOT NULL,
    action TEXT NOT NULL,
    timestamp TEXT NOT NULL,
    FOREIGN KEY (session_id) REFERENCES sessions (id),
    FOREIGN KEY (task_id) REFERENCES tasks (id)
); 