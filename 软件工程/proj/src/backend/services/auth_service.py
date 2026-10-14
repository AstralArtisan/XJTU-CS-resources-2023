from __future__ import annotations

from backend.database.db import get_connection
from backend.utils.auth import create_token
from backend.utils.password import hash_password, verify_password


def sanitize_user(row):
    return {
        "id": row["id"],
        "student_id": row["student_id"],
        "username": row["username"],
        "role": row["role"],
        "avatar_url": row["avatar_url"],
        "signature": row["signature"],
        "preference_text": row["preference_text"],
        "status": row["status"],
        "created_at": row["created_at"],
    }


def register_user(student_id: str, username: str, password: str):
    conn = get_connection()
    try:
        exists = conn.execute("SELECT id FROM user WHERE student_id = ?", (student_id,)).fetchone()
        if exists:
            return None, "该账号已存在"
        cursor = conn.execute(
            """
            INSERT INTO user (student_id, username, password_hash, role, status)
            VALUES (?, ?, ?, 0, 1)
            """,
            (student_id, username, hash_password(password)),
        )
        conn.commit()
        row = conn.execute("SELECT * FROM user WHERE id = ?", (cursor.lastrowid,)).fetchone()
        return sanitize_user(row), None
    finally:
        conn.close()


def login_user(student_id: str, password: str):
    conn = get_connection()
    try:
        row = conn.execute("SELECT * FROM user WHERE student_id = ?", (student_id,)).fetchone()
        if not row or not verify_password(password, row["password_hash"]) or row["status"] != 1:
            return None
        return {"token": create_token(row["id"], row["role"]), "user": sanitize_user(row)}
    finally:
        conn.close()


def get_user_by_id(user_id: int):
    conn = get_connection()
    try:
        row = conn.execute("SELECT * FROM user WHERE id = ?", (user_id,)).fetchone()
        return sanitize_user(row) if row else None
    finally:
        conn.close()


def change_password(user_id: int, old_password: str, new_password: str):
    conn = get_connection()
    try:
        row = conn.execute("SELECT * FROM user WHERE id = ?", (user_id,)).fetchone()
        if not row or not verify_password(old_password, row["password_hash"]):
            return False
        conn.execute(
            "UPDATE user SET password_hash = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
            (hash_password(new_password), user_id),
        )
        conn.commit()
        return True
    finally:
        conn.close()


def update_profile(user_id: int, data: dict):
    conn = get_connection()
    try:
        row = conn.execute("SELECT * FROM user WHERE id = ?", (user_id,)).fetchone()
        if not row:
            return None, "用户不存在"

        student_id = data.get("student_id", row["student_id"])
        if student_id != row["student_id"]:
            duplicate = conn.execute(
                "SELECT id FROM user WHERE student_id = ? AND id != ?",
                (student_id, user_id),
            ).fetchone()
            if duplicate:
                return None, "该账号已存在"

        merged = {
            "student_id": student_id,
            "username": data.get("username", row["username"]),
            "avatar_url": data.get("avatar_url", row["avatar_url"]),
            "signature": data.get("signature", row["signature"]),
            "preference_text": data.get("preference_text", row["preference_text"]),
        }
        conn.execute(
            """
            UPDATE user
            SET student_id = ?, username = ?, avatar_url = ?, signature = ?, preference_text = ?,
                updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            """,
            (
                merged["student_id"],
                merged["username"],
                merged["avatar_url"],
                merged["signature"],
                merged["preference_text"],
                user_id,
            ),
        )
        conn.commit()
        updated = conn.execute("SELECT * FROM user WHERE id = ?", (user_id,)).fetchone()
        return sanitize_user(updated), None
    finally:
        conn.close()

