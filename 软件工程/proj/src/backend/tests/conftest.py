"""
测试配置：使用内存数据库，每个测试函数独立隔离。
"""
import sys
import os
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", ".."))

import backend.database.db as db_module
from backend.database.db import init_db


@pytest.fixture(autouse=True)
def use_temp_db(tmp_path, monkeypatch):
    """每个测试使用独立的临时数据库，不插入种子数据。"""
    temp_db = tmp_path / "test_canteen.sqlite3"
    monkeypatch.setattr(db_module, "DB_PATH", temp_db)

    # 只建表和默认用户，不插入种子窗口数据
    from backend.database.db import get_connection
    import sqlite3
    conn = get_connection()
    conn.executescript("""
        CREATE TABLE IF NOT EXISTS user (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            student_id TEXT NOT NULL UNIQUE,
            username TEXT NOT NULL,
            password_hash TEXT NOT NULL,
            role INTEGER NOT NULL DEFAULT 0,
            status INTEGER NOT NULL DEFAULT 1,
            avatar_url TEXT DEFAULT NULL,
            signature TEXT DEFAULT NULL,
            preference_text TEXT DEFAULT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS canteen (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            location TEXT DEFAULT NULL,
            description TEXT DEFAULT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS stall (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            canteen_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            category TEXT DEFAULT NULL,
            description TEXT DEFAULT NULL,
            avg_rating REAL NOT NULL DEFAULT 0,
            review_count INTEGER NOT NULL DEFAULT 0,
            status INTEGER NOT NULL DEFAULT 1,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(canteen_id, name),
            FOREIGN KEY (canteen_id) REFERENCES canteen(id)
        );
        CREATE TABLE IF NOT EXISTS tag (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            description TEXT DEFAULT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS stall_tag (
            stall_id INTEGER NOT NULL,
            tag_id INTEGER NOT NULL,
            PRIMARY KEY (stall_id, tag_id),
            FOREIGN KEY (stall_id) REFERENCES stall(id) ON DELETE CASCADE,
            FOREIGN KEY (tag_id) REFERENCES tag(id) ON DELETE CASCADE
        );
        CREATE TABLE IF NOT EXISTS review (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            stall_id INTEGER NOT NULL,
            rating INTEGER NOT NULL,
            content TEXT DEFAULT NULL,
            is_deleted INTEGER NOT NULL DEFAULT 0,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(user_id, stall_id),
            FOREIGN KEY (user_id) REFERENCES user(id),
            FOREIGN KEY (stall_id) REFERENCES stall(id)
        );
        CREATE TABLE IF NOT EXISTS favorite (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            stall_id INTEGER NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(user_id, stall_id),
            FOREIGN KEY (user_id) REFERENCES user(id),
            FOREIGN KEY (stall_id) REFERENCES stall(id)
        );
        CREATE TABLE IF NOT EXISTS blacklist (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            stall_id INTEGER NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            UNIQUE(user_id, stall_id),
            FOREIGN KEY (user_id) REFERENCES user(id),
            FOREIGN KEY (stall_id) REFERENCES stall(id)
        );
        CREATE TABLE IF NOT EXISTS history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id INTEGER NOT NULL,
            stall_id INTEGER NOT NULL,
            visited_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (user_id) REFERENCES user(id),
            FOREIGN KEY (stall_id) REFERENCES stall(id)
        );
        PRAGMA foreign_keys = ON;
    """)
    conn.commit()
    conn.close()
    yield
