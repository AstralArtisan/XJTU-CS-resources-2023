from __future__ import annotations

from backend.database.db import CATEGORY_OPTIONS, get_connection


def _parse_tags(value):
    if not value:
        return []
    if isinstance(value, list):
        return [str(item).strip() for item in value if str(item).strip()]
    normalized = str(value).replace("，", ",")
    return [part.strip() for part in normalized.split(",") if part.strip()]


def _ensure_tags(conn, tags):
    tag_ids = []
    seen_names = set()
    for tag_name in _parse_tags(tags):
        if tag_name in seen_names:
            continue
        seen_names.add(tag_name)
        existing = conn.execute("SELECT id FROM tag WHERE name = ?", (tag_name,)).fetchone()
        if existing:
            tag_ids.append(existing["id"])
        else:
            cursor = conn.execute(
                "INSERT INTO tag (name, description) VALUES (?, ?)",
                (tag_name, f"{tag_name} 标签"),
            )
            tag_ids.append(cursor.lastrowid)
    return tag_ids


def _replace_stall_tags(conn, stall_id: int, tags):
    conn.execute("DELETE FROM stall_tag WHERE stall_id = ?", (stall_id,))
    tag_ids = list(dict.fromkeys(_ensure_tags(conn, tags)))
    conn.executemany(
        "INSERT INTO stall_tag (stall_id, tag_id) VALUES (?, ?)",
        [(stall_id, tag_id) for tag_id in tag_ids],
    )


def _fetch_tag_names(conn, stall_id: int):
    rows = conn.execute(
        """
        SELECT t.name
        FROM stall_tag st
        JOIN tag t ON t.id = st.tag_id
        WHERE st.stall_id = ?
        ORDER BY t.name
        """,
        (stall_id,),
    ).fetchall()
    return [row["name"] for row in rows]


def list_categories():
    conn = get_connection()
    try:
        existing = [row["category"] for row in conn.execute("SELECT DISTINCT category FROM stall WHERE category IS NOT NULL").fetchall()]
        ordered = [item for item in CATEGORY_OPTIONS if item in existing]
        extra = sorted([item for item in existing if item not in CATEGORY_OPTIONS])
        return ordered + extra
    finally:
        conn.close()


def query_stalls(page=1, page_size=10, canteen_id=None, category=None, keyword=None, sort_by=None, tag_name=None):
    conn = get_connection()
    try:
        where = [" AND s.status = 1"]
        params = []
        join_tag = ""
        if canteen_id:
            where.append(" AND s.canteen_id = ?")
            params.append(canteen_id)
        if category:
            where.append(" AND s.category = ?")
            params.append(category)
        if keyword:
            where.append(" AND (s.name LIKE ? OR s.description LIKE ?)")
            params.extend([f"%{keyword}%", f"%{keyword}%"])
        if tag_name:
            join_tag = """
                JOIN stall_tag st ON st.stall_id = s.id
                JOIN tag t ON t.id = st.tag_id
            """
            where.append(" AND t.name = ?")
            params.append(tag_name)

        order_clause = " ORDER BY s.id DESC"
        if sort_by == "score":
            order_clause = " ORDER BY s.avg_rating DESC, s.review_count DESC, s.id DESC"
        elif sort_by == "hot":
            order_clause = " ORDER BY s.review_count DESC, s.avg_rating DESC, s.id DESC"

        base = f"""
            FROM stall s
            JOIN canteen c ON c.id = s.canteen_id
            {join_tag}
            WHERE 1 = 1
        """ + "".join(where)

        total = conn.execute("SELECT COUNT(DISTINCT s.id) " + base, params).fetchone()[0]
        rows = conn.execute(
            """
            SELECT DISTINCT s.id, s.canteen_id, c.name AS canteen_name, s.name, s.category,
                   s.description, ROUND(s.avg_rating, 2) AS avg_rating, s.review_count, s.status
            """
            + base
            + order_clause
            + " LIMIT ? OFFSET ?",
            params + [page_size, (page - 1) * page_size],
        ).fetchall()
        items = []
        for row in rows:
            item = dict(row)
            item["tags"] = _fetch_tag_names(conn, row["id"])
            items.append(item)
        return items, total
    finally:
        conn.close()


def get_stall_detail(stall_id: int):
    conn = get_connection()
    try:
        row = conn.execute(
            """
            SELECT s.id, s.canteen_id, c.name AS canteen_name, s.name, s.category, s.description,
                   ROUND(s.avg_rating, 2) AS avg_rating, s.review_count, s.status
            FROM stall s
            JOIN canteen c ON c.id = s.canteen_id
            WHERE s.id = ?
            """,
            (stall_id,),
        ).fetchone()
        if not row:
            return None
        item = dict(row)
        item["tags"] = _fetch_tag_names(conn, stall_id)
        return item
    finally:
        conn.close()


def create_stall(data: dict):
    conn = get_connection()
    try:
        cursor = conn.execute(
            "INSERT INTO stall (canteen_id, name, category, description) VALUES (?, ?, ?, ?)",
            (data["canteen_id"], data["name"], data.get("category"), data.get("description")),
        )
        if "tags" in data:
            _replace_stall_tags(conn, cursor.lastrowid, data.get("tags"))
        conn.commit()
        return get_stall_detail(cursor.lastrowid)
    finally:
        conn.close()


def update_stall(stall_id: int, data: dict):
    conn = get_connection()
    try:
        row = conn.execute("SELECT * FROM stall WHERE id = ?", (stall_id,)).fetchone()
        if not row:
            return None
        merged = {
            "canteen_id": data.get("canteen_id", row["canteen_id"]),
            "name": data.get("name", row["name"]),
            "category": data.get("category", row["category"]),
            "description": data.get("description", row["description"]),
            "status": data.get("status", row["status"]),
        }
        conn.execute(
            """
            UPDATE stall
            SET canteen_id = ?, name = ?, category = ?, description = ?, status = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            """,
            (merged["canteen_id"], merged["name"], merged["category"], merged["description"], merged["status"], stall_id),
        )
        if "tags" in data:
            _replace_stall_tags(conn, stall_id, data.get("tags"))
        conn.commit()
        return get_stall_detail(stall_id)
    finally:
        conn.close()


def disable_stall(stall_id: int):
    return update_stall(stall_id, {"status": 0})


def list_tags():
    conn = get_connection()
    try:
        rows = conn.execute("SELECT id, name, description, created_at FROM tag ORDER BY name").fetchall()
        return [dict(row) for row in rows]
    finally:
        conn.close()


def create_tag(data: dict):
    conn = get_connection()
    try:
        existing = conn.execute(
            "SELECT id, name, description, created_at FROM tag WHERE name = ?",
            (data["name"],),
        ).fetchone()
        if existing:
            return dict(existing)
        cursor = conn.execute(
            "INSERT INTO tag (name, description) VALUES (?, ?)",
            (data["name"], data.get("description")),
        )
        conn.commit()
        row = conn.execute(
            "SELECT id, name, description, created_at FROM tag WHERE id = ?",
            (cursor.lastrowid,),
        ).fetchone()
        return dict(row)
    finally:
        conn.close()


def update_tag(tag_id: int, data: dict):
    conn = get_connection()
    try:
        row = conn.execute("SELECT * FROM tag WHERE id = ?", (tag_id,)).fetchone()
        if not row:
            return None
        merged = {
            "name": data.get("name", row["name"]),
            "description": data.get("description", row["description"]),
        }
        duplicate = conn.execute(
            "SELECT id FROM tag WHERE name = ? AND id != ?",
            (merged["name"], tag_id),
        ).fetchone()
        if duplicate:
            duplicate_row = conn.execute(
                "SELECT id, name, description, created_at FROM tag WHERE id = ?",
                (duplicate["id"],),
            ).fetchone()
            return dict(duplicate_row)
        conn.execute(
            """
            UPDATE tag
            SET name = ?, description = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = ?
            """,
            (merged["name"], merged["description"], tag_id),
        )
        conn.commit()
        updated = conn.execute(
            "SELECT id, name, description, created_at FROM tag WHERE id = ?",
            (tag_id,),
        ).fetchone()
        return dict(updated)
    finally:
        conn.close()

