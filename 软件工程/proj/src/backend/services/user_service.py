from __future__ import annotations

from backend.database.db import get_connection


def _paginate_rows(base_sql: str, count_sql: str, params: list, page: int, page_size: int):
    conn = get_connection()
    try:
        total = conn.execute(count_sql, params).fetchone()[0]
        rows = conn.execute(base_sql + " LIMIT ? OFFSET ?", params + [page_size, (page - 1) * page_size]).fetchall()
        return [dict(row) for row in rows], total
    finally:
        conn.close()


def add_favorite(user_id: int, stall_id: int):
    conn = get_connection()
    try:
        conn.execute("INSERT OR IGNORE INTO favorite (user_id, stall_id) VALUES (?, ?)", (user_id, stall_id))
        conn.commit()
        row = conn.execute(
            "SELECT id, user_id, stall_id, created_at FROM favorite WHERE user_id = ? AND stall_id = ?",
            (user_id, stall_id),
        ).fetchone()
        return dict(row)
    finally:
        conn.close()


def remove_favorite(user_id: int, stall_id: int):
    conn = get_connection()
    try:
        conn.execute("DELETE FROM favorite WHERE user_id = ? AND stall_id = ?", (user_id, stall_id))
        conn.commit()
        return {"result": "success"}
    finally:
        conn.close()


def list_favorites(user_id: int, page=1, page_size=10):
    sql = """
        SELECT s.id AS stall_id, s.name AS stall_name, c.name AS canteen_name,
               ROUND(s.avg_rating, 2) AS avg_rating, s.review_count, f.created_at
        FROM favorite f
        JOIN stall s ON s.id = f.stall_id
        JOIN canteen c ON c.id = s.canteen_id
        WHERE f.user_id = ?
        ORDER BY f.created_at DESC, f.id DESC
    """
    return _paginate_rows(sql, "SELECT COUNT(*) FROM favorite WHERE user_id = ?", [user_id], page, page_size)


def add_blacklist(user_id: int, stall_id: int):
    conn = get_connection()
    try:
        conn.execute("INSERT OR IGNORE INTO blacklist (user_id, stall_id) VALUES (?, ?)", (user_id, stall_id))
        conn.commit()
        row = conn.execute(
            "SELECT id, user_id, stall_id, created_at FROM blacklist WHERE user_id = ? AND stall_id = ?",
            (user_id, stall_id),
        ).fetchone()
        return dict(row)
    finally:
        conn.close()


def remove_blacklist(user_id: int, stall_id: int):
    """Remove a stall from the user's blacklist."""
    conn = get_connection()
    try:
        conn.execute("DELETE FROM blacklist WHERE user_id = ? AND stall_id = ?", (user_id, stall_id))
        conn.commit()
        return {"result": "success"}
    finally:
        conn.close()


def list_blacklist(user_id: int, page=1, page_size=10):
    """List blacklisted stalls for a user with pagination."""
    sql = """
        SELECT s.id AS stall_id, s.name AS stall_name, c.name AS canteen_name, b.created_at
        FROM blacklist b
        JOIN stall s ON s.id = b.stall_id
        JOIN canteen c ON c.id = s.canteen_id
        WHERE b.user_id = ?
        ORDER BY b.created_at DESC, b.id DESC
    """
    return _paginate_rows(sql, "SELECT COUNT(*) FROM blacklist WHERE user_id = ?", [user_id], page, page_size)


def add_history(user_id: int, stall_id: int):
    """Record a stall visit, deduplicating within a 30-minute window."""
    conn = get_connection()
    try:
        recent = conn.execute(
            """
            SELECT id FROM history
            WHERE user_id = ? AND stall_id = ? AND visited_at >= datetime('now', '-30 minutes')
            ORDER BY visited_at DESC, id DESC
            LIMIT 1
            """,
            (user_id, stall_id),
        ).fetchone()
        if recent:
            return {"result": "success"}
        conn.execute("INSERT INTO history (user_id, stall_id) VALUES (?, ?)", (user_id, stall_id))
        conn.commit()
        return {"result": "success"}
    finally:
        conn.close()


def list_history(user_id: int, page=1, page_size=10):
    sql = """
        SELECT s.id AS stall_id, s.name AS stall_name, c.name AS canteen_name, h.visited_at
        FROM history h
        JOIN stall s ON s.id = h.stall_id
        JOIN canteen c ON c.id = s.canteen_id
        WHERE h.user_id = ?
        ORDER BY h.visited_at DESC, h.id DESC
    """
    return _paginate_rows(sql, "SELECT COUNT(*) FROM history WHERE user_id = ?", [user_id], page, page_size)


def list_users():
    conn = get_connection()
    try:
        rows = conn.execute(
            """
            SELECT id, student_id, username, role, status, avatar_url, signature, created_at
            FROM user
            ORDER BY role DESC, id ASC
            """
        ).fetchall()
        return [dict(row) for row in rows]
    finally:
        conn.close()


def update_user_role(user_id: int, role: int):
    conn = get_connection()
    try:
        row = conn.execute("SELECT * FROM user WHERE id = ?", (user_id,)).fetchone()
        if not row:
            return None
        conn.execute(
            "UPDATE user SET role = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
            (role, user_id),
        )
        conn.commit()
        updated = conn.execute(
            "SELECT id, student_id, username, role, status, avatar_url, signature, created_at FROM user WHERE id = ?",
            (user_id,),
        ).fetchone()
        return dict(updated)
    finally:
        conn.close()
