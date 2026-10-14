from __future__ import annotations

from backend.database.db import get_connection
from backend.services.stall_service import get_stall_detail


def recalculate_stall_stats(stall_id: int):
    conn = get_connection()
    try:
        row = conn.execute(
            """
            SELECT COUNT(*) AS review_count, COALESCE(AVG(rating), 0) AS avg_rating
            FROM review
            WHERE stall_id = ? AND is_deleted = 0
            """,
            (stall_id,),
        ).fetchone()
        conn.execute(
            "UPDATE stall SET review_count = ?, avg_rating = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
            (row["review_count"], round(row["avg_rating"], 2), stall_id),
        )
        conn.commit()
    finally:
        conn.close()


def create_or_update_review(user_id: int, stall_id: int, rating: int, content: str | None):
    if not get_stall_detail(stall_id):
        return None
    conn = get_connection()
    try:
        row = conn.execute("SELECT * FROM review WHERE user_id = ? AND stall_id = ?", (user_id, stall_id)).fetchone()
        if row:
            conn.execute(
                """
                UPDATE review
                SET rating = ?, content = ?, is_deleted = 0, updated_at = CURRENT_TIMESTAMP
                WHERE id = ?
                """,
                (rating, content, row["id"]),
            )
            review_id = row["id"]
        else:
            cursor = conn.execute(
                "INSERT INTO review (user_id, stall_id, rating, content) VALUES (?, ?, ?, ?)",
                (user_id, stall_id, rating, content),
            )
            review_id = cursor.lastrowid
        conn.commit()
        review = conn.execute("SELECT * FROM review WHERE id = ?", (review_id,)).fetchone()
    finally:
        conn.close()
    recalculate_stall_stats(stall_id)
    return dict(review)


def get_reviews_of_stall(stall_id: int, page=1, page_size=10, sort_by="latest"):
    conn = get_connection()
    try:
        order_clause = " ORDER BY r.created_at DESC, r.id DESC"
        if sort_by == "score_desc":
            order_clause = " ORDER BY r.rating DESC, r.created_at DESC, r.id DESC"
        total = conn.execute(
            "SELECT COUNT(*) FROM review WHERE stall_id = ? AND is_deleted = 0",
            (stall_id,),
        ).fetchone()[0]
        rows = conn.execute(
            """
            SELECT r.id, r.user_id, u.username, r.rating, r.content, r.created_at, r.updated_at
            FROM review r
            JOIN user u ON u.id = r.user_id
            WHERE r.stall_id = ? AND r.is_deleted = 0
            """
            + order_clause
            + " LIMIT ? OFFSET ?",
            (stall_id, page_size, (page - 1) * page_size),
        ).fetchall()
        return [dict(row) for row in rows], total
    finally:
        conn.close()


def get_reviews_of_user(user_id: int, page=1, page_size=10):
    conn = get_connection()
    try:
        total = conn.execute("SELECT COUNT(*) FROM review WHERE user_id = ? AND is_deleted = 0", (user_id,)).fetchone()[0]
        rows = conn.execute(
            """
            SELECT r.id, r.user_id, r.stall_id, s.name AS stall_name, c.name AS canteen_name,
                   r.rating, r.content, r.created_at, r.updated_at
            FROM review r
            JOIN stall s ON s.id = r.stall_id
            JOIN canteen c ON c.id = s.canteen_id
            WHERE r.user_id = ? AND r.is_deleted = 0
            ORDER BY r.updated_at DESC, r.id DESC
            LIMIT ? OFFSET ?
            """,
            (user_id, page_size, (page - 1) * page_size),
        ).fetchall()
        return [dict(row) for row in rows], total
    finally:
        conn.close()


def update_own_review(user_id: int, review_id: int, rating: int, content: str | None):
    conn = get_connection()
    try:
        row = conn.execute(
            "SELECT * FROM review WHERE id = ? AND user_id = ? AND is_deleted = 0",
            (review_id, user_id),
        ).fetchone()
        if not row:
            return None
        conn.execute(
            "UPDATE review SET rating = ?, content = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
            (rating, content, review_id),
        )
        conn.commit()
        updated = conn.execute("SELECT * FROM review WHERE id = ?", (review_id,)).fetchone()
        stall_id = row["stall_id"]
    finally:
        conn.close()
    recalculate_stall_stats(stall_id)
    return dict(updated)


def soft_delete_review(review_id: int):
    conn = get_connection()
    try:
        row = conn.execute("SELECT * FROM review WHERE id = ?", (review_id,)).fetchone()
        if not row:
            return None
        conn.execute("UPDATE review SET is_deleted = 1, updated_at = CURRENT_TIMESTAMP WHERE id = ?", (review_id,))
        conn.commit()
        stall_id = row["stall_id"]
    finally:
        conn.close()
    recalculate_stall_stats(stall_id)
    return {"result": "success"}
