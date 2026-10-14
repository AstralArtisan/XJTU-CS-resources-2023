from __future__ import annotations

from backend.database.db import get_connection


def get_all_canteens():
    conn = get_connection()
    try:
        rows = conn.execute("SELECT id, name, location, description FROM canteen ORDER BY id").fetchall()
        return [dict(row) for row in rows]
    finally:
        conn.close()


def get_canteen_detail(canteen_id: int):
    conn = get_connection()
    try:
        row = conn.execute(
            "SELECT id, name, location, description FROM canteen WHERE id = ?",
            (canteen_id,),
        ).fetchone()
        return dict(row) if row else None
    finally:
        conn.close()


def create_canteen(data: dict):
    conn = get_connection()
    try:
        cursor = conn.execute(
            "INSERT INTO canteen (name, location, description) VALUES (?, ?, ?)",
            (data["name"], data.get("location"), data.get("description")),
        )
        conn.commit()
        return get_canteen_detail(cursor.lastrowid)
    finally:
        conn.close()


def update_canteen(canteen_id: int, data: dict):
    conn = get_connection()
    try:
        row = conn.execute("SELECT * FROM canteen WHERE id = ?", (canteen_id,)).fetchone()
        if not row:
            return None
        merged = {
            "name": data.get("name", row["name"]),
            "location": data.get("location", row["location"]),
            "description": data.get("description", row["description"]),
        }
        conn.execute(
            """
            UPDATE canteen
            SET name = ?, location = ?, description = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            """,
            (merged["name"], merged["location"], merged["description"], canteen_id),
        )
        conn.commit()
        return get_canteen_detail(canteen_id)
    finally:
        conn.close()
