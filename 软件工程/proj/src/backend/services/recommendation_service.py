from __future__ import annotations

from backend.database.db import get_connection


def _split_keywords(text: str):
    normalized = (
        (text or "")
        .replace("，", " ")
        .replace(",", " ")
        .replace("、", " ")
        .replace("；", " ")
        .replace(";", " ")
    )
    return [part.strip() for part in normalized.split() if part.strip()]


def _user_context(conn, user_id: int):
    context = {
        "liked_stall_ids": set(),
        "blacklist_ids": set(),
        "keywords": [],
        "preferred_tags": set(),
    }
    if not user_id:
        return context

    context["liked_stall_ids"] = {
        row["stall_id"]
        for row in conn.execute("SELECT stall_id FROM favorite WHERE user_id = ?", (user_id,)).fetchall()
    }
    context["blacklist_ids"] = {
        row["stall_id"]
        for row in conn.execute("SELECT stall_id FROM blacklist WHERE user_id = ?", (user_id,)).fetchall()
    }

    reviewed_good = conn.execute(
        "SELECT stall_id FROM review WHERE user_id = ? AND is_deleted = 0 AND rating >= 4",
        (user_id,),
    ).fetchall()
    context["liked_stall_ids"].update(row["stall_id"] for row in reviewed_good)

    recent_history = conn.execute(
        """
        SELECT stall_id
        FROM history
        WHERE user_id = ?
        ORDER BY visited_at DESC, id DESC
        LIMIT 8
        """,
        (user_id,),
    ).fetchall()
    context["liked_stall_ids"].update(row["stall_id"] for row in recent_history)

    user = conn.execute("SELECT preference_text FROM user WHERE id = ?", (user_id,)).fetchone()
    if user and user["preference_text"]:
        context["keywords"] = _split_keywords(user["preference_text"])

    tag_rows = conn.execute(
        """
        SELECT DISTINCT t.name
        FROM review r
        JOIN stall_tag st ON st.stall_id = r.stall_id
        JOIN tag t ON t.id = st.tag_id
        WHERE r.user_id = ? AND r.is_deleted = 0 AND r.rating >= 4
        """,
        (user_id,),
    ).fetchall()
    context["preferred_tags"] = {row["name"] for row in tag_rows}
    return context


def _candidate_stalls(conn, canteen_id=None, category=None):
    params = []
    where = [" WHERE s.status = 1 "]
    if canteen_id:
        where.append(" AND s.canteen_id = ?")
        params.append(canteen_id)
    if category:
        where.append(" AND s.category = ?")
        params.append(category)

    rows = conn.execute(
        """
        SELECT s.id AS stall_id, s.name AS stall_name, c.name AS canteen_name, s.category,
               ROUND(s.avg_rating, 2) AS avg_rating, s.review_count, s.description
        FROM stall s
        JOIN canteen c ON c.id = s.canteen_id
        """
        + "".join(where),
        params,
    ).fetchall()

    items = []
    for row in rows:
        item = dict(row)
        tag_rows = conn.execute(
            """
            SELECT t.name
            FROM stall_tag st
            JOIN tag t ON t.id = st.tag_id
            WHERE st.stall_id = ?
            ORDER BY t.name
            """,
            (row["stall_id"],),
        ).fetchall()
        item["tags"] = [tag["name"] for tag in tag_rows]
        items.append(item)
    return items


def _score_candidate(item, context, extra_keywords):
    score = item["avg_rating"] * 20 + item["review_count"] * 6
    reasons = []

    if item["stall_id"] in context["liked_stall_ids"]:
        score += 20
        reasons.append("你收藏或评过高分的窗口里有类似的，应该对口")

    matched_tags = [tag for tag in item["tags"] if tag in context["preferred_tags"]]
    if matched_tags:
        score += 12 + len(matched_tags) * 4
        reasons.append(f"符合你的口味偏好：{'、'.join(matched_tags)}")

    keywords = context["keywords"] + extra_keywords
    text = f"{item['stall_name']} {item['category'] or ''} {item['description'] or ''} {' '.join(item['tags'])}"
    hits = [word for word in keywords if word and word in text]
    if hits:
        score += 10 + len(hits) * 3
        reasons.append(f"你提到的「{'、'.join(dict.fromkeys(hits))}」这里都有")

    if item["avg_rating"] >= 4.6:
        score += 6
        reasons.append("评分很高，口碑不错")
    elif item["avg_rating"] >= 4.3:
        reasons.append("评分挺好的，吃过的人大多满意")

    if item["review_count"] >= 3:
        score += 4
        reasons.append("评价的人不少，可以参考一下")

    return score, reasons or ["根据评分和热度推荐给你，可以试试"]


def _rank_candidates(user_id=None, preference_text="", canteen_id=None, category=None, exclude_blacklist=False):
    keywords = _split_keywords(preference_text)
    conn = get_connection()
    try:
        context = _user_context(conn, user_id)
        candidates = _candidate_stalls(conn, canteen_id, category)
        result = []
        for item in candidates:
            if user_id and item["stall_id"] in context["blacklist_ids"]:
                continue
            if exclude_blacklist and user_id and item["stall_id"] in context["blacklist_ids"]:
                continue
            score, reasons = _score_candidate(item, context, keywords)
            result.append(
                {
                    "stall_id": item["stall_id"],
                    "stall_name": item["stall_name"],
                    "canteen_name": item["canteen_name"],
                    "category": item["category"],
                    "avg_rating": item["avg_rating"],
                    "review_count": item["review_count"],
                    "tags": item["tags"],
                    "reason": "；".join(reasons[:3]),
                    "match_score": min(100, int(score)),
                }
            )
        result.sort(key=lambda x: (-x["match_score"], -x["review_count"], x["stall_id"]))
        return result, context
    finally:
        conn.close()


def recommend_today(canteen_id=None, category=None, exclude_blacklist=False, user_id=None, limit=3, seed=0):
    ranked, context = _rank_candidates(
        user_id=user_id,
        preference_text="",
        canteen_id=canteen_id,
        category=category,
        exclude_blacklist=exclude_blacklist,
    )
    if len(ranked) > limit:
        offset = (len(context["liked_stall_ids"]) + len(context["preferred_tags"]) + int(seed)) % len(ranked)
        doubled = ranked + ranked
        ranked = doubled[offset : offset + limit]
    else:
        ranked = ranked[:limit]
    return ranked


def recommend_personalized(user_id=None, preference_text="", exclude_blacklist=False, limit=5):
    ranked, _ = _rank_candidates(
        user_id=user_id,
        preference_text=preference_text,
        exclude_blacklist=exclude_blacklist,
    )
    return ranked[:limit]


def recommend_feed(user_id=None, preference_text="", canteen_id=None, category=None, exclude_blacklist=True, limit=5, seed=0):
    ranked, context = _rank_candidates(
        user_id=user_id,
        preference_text=preference_text,
        canteen_id=canteen_id,
        category=category,
        exclude_blacklist=exclude_blacklist,
    )
    if len(ranked) > limit:
        offset = (int(seed) + len(context["liked_stall_ids"])) % len(ranked)
        doubled = ranked + ranked
        ranked = doubled[offset : offset + limit]
    else:
        ranked = ranked[:limit]
    return ranked

