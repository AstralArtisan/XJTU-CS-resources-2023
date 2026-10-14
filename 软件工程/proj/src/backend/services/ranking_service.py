from __future__ import annotations

from backend.database.db import get_connection


def _filter(canteen_id):
    clauses = ["s.status = 1"]
    params = []
    if canteen_id:
        clauses.append("s.canteen_id = ?")
        params.append(canteen_id)
    return " WHERE " + " AND ".join(clauses) + " ", params


def query_score_ranking(limit=10, canteen_id=None):
    conn = get_connection()
    try:
        where_sql, params = _filter(canteen_id)
        rows = conn.execute(
            """
            SELECT s.id AS stall_id, s.name AS stall_name, s.canteen_id, c.name AS canteen_name,
                   ROUND(s.avg_rating, 2) AS avg_rating, s.review_count
            FROM stall s
            JOIN canteen c ON c.id = s.canteen_id
            """
            + where_sql
            + " ORDER BY s.avg_rating DESC, s.review_count DESC, s.id DESC LIMIT ?",
            params + [limit],
        ).fetchall()
        return [dict(row) for row in rows]
    finally:
        conn.close()


def query_hot_ranking(limit=10, canteen_id=None):
    conn = get_connection()
    try:
        where_sql, params = _filter(canteen_id)
        rows = conn.execute(
            """
            SELECT s.id AS stall_id, s.name AS stall_name, s.canteen_id, c.name AS canteen_name,
                   ROUND(s.avg_rating, 2) AS avg_rating, s.review_count
            FROM stall s
            JOIN canteen c ON c.id = s.canteen_id
            """
            + where_sql
            + " ORDER BY s.review_count DESC, s.avg_rating DESC, s.id DESC LIMIT ?",
            params + [limit],
        ).fetchall()
        return [dict(row) for row in rows]
    finally:
        conn.close()


def query_latest_ranking(limit=10, canteen_id=None):
    conn = get_connection()
    try:
        where_sql, params = _filter(canteen_id)
        rows = conn.execute(
            """
            SELECT s.id AS stall_id, s.name AS stall_name, c.name AS canteen_name,
                   MAX(r.updated_at) AS latest_review_time,
                   ROUND(s.avg_rating, 2) AS avg_rating, s.review_count
            FROM stall s
            JOIN canteen c ON c.id = s.canteen_id
            LEFT JOIN review r ON r.stall_id = s.id AND r.is_deleted = 0
            """
            + where_sql
            + """
            GROUP BY s.id, s.name, c.name, s.avg_rating, s.review_count
            ORDER BY latest_review_time DESC, s.id DESC
            LIMIT ?
            """,
            params + [limit],
        ).fetchall()
        return [dict(row) for row in rows]
    finally:
        conn.close()
