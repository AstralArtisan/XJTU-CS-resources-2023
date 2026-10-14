from __future__ import annotations

import json
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

from backend.database.db import init_db
from backend.services import (
    admin_service,
    auth_service,
    canteen_service,
    llm_service,
    ranking_service,
    recommendation_service,
    review_service,
    stall_service,
    user_service,
)
from backend.utils.auth import parse_token
from backend.utils.errors import error_payload
from backend.utils.response import page_success, success

HOST = "127.0.0.1"
PORT = 8000


def _json(handler, payload, status=200):
    body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Access-Control-Allow-Origin", "*")
    handler.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
    handler.send_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
    handler.send_header("Content-Length", str(len(body)))
    handler.end_headers()
    handler.wfile.write(body)


def _read_json(handler):
    content_length = int(handler.headers.get("Content-Length", "0"))
    if content_length <= 0:
        return {}
    return json.loads(handler.rfile.read(content_length).decode("utf-8"))


def _query(handler):
    values = parse_qs(urlparse(handler.path).query)
    return {k: v[-1] for k, v in values.items()}


def _auth_user(handler, require_login=True):
    auth_header = handler.headers.get("Authorization", "")
    if not auth_header.startswith("Bearer "):
        return None if not require_login else ("error", error_payload("not_login"))
    payload = parse_token(auth_header[7:])
    if not payload:
        return None if not require_login else ("error", error_payload("not_login"))
    user = auth_service.get_user_by_id(payload["user_id"])
    if not user:
        return ("error", error_payload("not_login"))
    return user


def _require_role(handler, min_role=1):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return user
    if user["role"] < min_role:
        return "error", error_payload("forbidden")
    return user


class AppHandler(BaseHTTPRequestHandler):
    def do_OPTIONS(self):
        _json(self, success({"result": "ok"}))

    def do_GET(self):
        self.dispatch("GET")

    def do_POST(self):
        self.dispatch("POST")

    def do_PUT(self):
        self.dispatch("PUT")

    def do_DELETE(self):
        self.dispatch("DELETE")

    def log_message(self, format, *args):
        return

    def dispatch(self, method):
        path = urlparse(self.path).path
        for route_method, pattern, handler in ROUTES:
            if route_method != method:
                continue
            match = re.fullmatch(pattern, path)
            if not match:
                continue
            try:
                payload = handler(self, **match.groupdict())
                if isinstance(payload, tuple) and len(payload) == 2:
                    _json(self, payload[1], payload[0])
                else:
                    _json(self, payload)
            except Exception as exc:
                _json(self, {"code": 5000, "message": f"server_error: {exc}", "data": None}, 500)
            return
        _json(self, error_payload("not_found"), 404)


def register(handler):
    data = _read_json(handler)
    if not data.get("student_id") or not data.get("username") or not data.get("password"):
        return 400, {"code": 4010, "message": "账号、昵称和密码不能为空", "data": None}
    user, error = auth_service.register_user(data["student_id"], data["username"], data["password"])
    return success(user) if not error else (400, {"code": 4010, "message": error, "data": None})


def login(handler):
    data = _read_json(handler)
    result = auth_service.login_user(data.get("student_id", ""), data.get("password", ""))
    return success(result) if result else (400, {"code": 4010, "message": "账号或密码错误", "data": None})


def get_me(handler):
    user = _auth_user(handler)
    return success(user) if not isinstance(user, tuple) else (401, user[1])


def logout(handler):
    return success({"result": "success"}) if not isinstance(_auth_user(handler), tuple) else (401, error_payload("not_login"))


def change_password(handler):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    data = _read_json(handler)
    ok = auth_service.change_password(user["id"], data.get("old_password", ""), data.get("new_password", ""))
    return success({"result": "success"}) if ok else (400, {"code": 4010, "message": "原密码错误", "data": None})


def update_profile(handler):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    item, error = auth_service.update_profile(user["id"], _read_json(handler))
    return success(item) if not error else (400, {"code": 4010, "message": error, "data": None})


def list_canteens(handler):
    return success({"list": canteen_service.get_all_canteens()})


def get_canteen(handler, id):
    item = canteen_service.get_canteen_detail(int(id))
    return success(item) if item else (404, error_payload("not_found"))


def list_categories(handler):
    return success({"list": stall_service.list_categories()})


def list_tags(handler):
    return success({"list": stall_service.list_tags()})


def list_stalls(handler):
    q = _query(handler)
    page = int(q.get("page", 1))
    page_size = int(q.get("page_size", 10))
    items, total = stall_service.query_stalls(
        page=page,
        page_size=page_size,
        canteen_id=int(q["canteen_id"]) if q.get("canteen_id") else None,
        category=q.get("category"),
        keyword=q.get("keyword"),
        sort_by=q.get("sort_by"),
        tag_name=q.get("tag_name"),
    )
    return page_success(items, total, page, page_size)


def get_stall(handler, id):
    item = stall_service.get_stall_detail(int(id))
    return success(item) if item else (404, error_payload("not_found"))


def submit_review(handler):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    data = _read_json(handler)
    if data.get("rating") not in [1, 2, 3, 4, 5]:
        return 400, {"code": 4010, "message": "评分必须是 1 到 5", "data": None}
    review = review_service.create_or_update_review(
        user["id"],
        int(data["stall_id"]),
        int(data["rating"]),
        data.get("content"),
    )
    return success(review) if review else (404, error_payload("not_found"))


def list_stall_reviews(handler, id):
    q = _query(handler)
    page = int(q.get("page", 1))
    page_size = int(q.get("page_size", 10))
    items, total = review_service.get_reviews_of_stall(int(id), page, page_size, q.get("sort_by", "latest"))
    return page_success(items, total, page, page_size)


def score_ranking(handler):
    q = _query(handler)
    items = ranking_service.query_score_ranking(int(q.get("limit", 10)), int(q["canteen_id"]) if q.get("canteen_id") else None)
    return success({"list": items})


def hot_ranking(handler):
    q = _query(handler)
    items = ranking_service.query_hot_ranking(int(q.get("limit", 10)), int(q["canteen_id"]) if q.get("canteen_id") else None)
    return success({"list": items})


def latest_ranking(handler):
    q = _query(handler)
    items = ranking_service.query_latest_ranking(int(q.get("limit", 10)), int(q["canteen_id"]) if q.get("canteen_id") else None)
    return success({"list": items})


def list_my_reviews(handler):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    q = _query(handler)
    page = int(q.get("page", 1))
    page_size = int(q.get("page_size", 10))
    items, total = review_service.get_reviews_of_user(user["id"], page, page_size)
    return page_success(items, total, page, page_size)


def update_my_review(handler, id):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    data = _read_json(handler)
    item = review_service.update_own_review(user["id"], int(id), int(data["rating"]), data.get("content"))
    return success(item) if item else (404, error_payload("not_found"))


def delete_my_review(handler, id):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    mine = review_service.get_reviews_of_user(user["id"], 1, 1000)[0]
    if not any(item["id"] == int(id) for item in mine):
        return 404, error_payload("not_found")
    return success(review_service.soft_delete_review(int(id)))


def add_favorite(handler):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    return success(user_service.add_favorite(user["id"], int(_read_json(handler)["stall_id"])))


def delete_favorite(handler, stall_id):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    return success(user_service.remove_favorite(user["id"], int(stall_id)))


def list_favorites(handler):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    q = _query(handler)
    page = int(q.get("page", 1))
    page_size = int(q.get("page_size", 10))
    items, total = user_service.list_favorites(user["id"], page, page_size)
    return page_success(items, total, page, page_size)


def add_blacklist(handler):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    return success(user_service.add_blacklist(user["id"], int(_read_json(handler)["stall_id"])))


def delete_blacklist(handler, stall_id):
    """Remove a stall from the current user's blacklist."""
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    return success(user_service.remove_blacklist(user["id"], int(stall_id)))


def list_blacklist(handler):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    q = _query(handler)
    page = int(q.get("page", 1))
    page_size = int(q.get("page_size", 10))
    items, total = user_service.list_blacklist(user["id"], page, page_size)
    return page_success(items, total, page, page_size)


def list_history(handler):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    q = _query(handler)
    page = int(q.get("page", 1))
    page_size = int(q.get("page_size", 10))
    items, total = user_service.list_history(user["id"], page, page_size)
    return page_success(items, total, page, page_size)


def add_history(handler):
    user = _auth_user(handler)
    if isinstance(user, tuple):
        return 401, user[1]
    return success(user_service.add_history(user["id"], int(_read_json(handler)["stall_id"])))


def recommend_today(handler):
    q = _query(handler)
    user = _auth_user(handler, require_login=False)
    user_id = None if isinstance(user, tuple) or user is None else user["id"]
    items = recommendation_service.recommend_today(
        canteen_id=int(q["canteen_id"]) if q.get("canteen_id") else None,
        category=q.get("category"),
        exclude_blacklist=q.get("exclude_blacklist") == "true",
        user_id=user_id,
        limit=int(q.get("limit", 3)),
        seed=int(q.get("seed", 0)),
    )
    return success({"list": items})


def recommend_personalized(handler):
    user = _auth_user(handler, require_login=False)
    user_id = None if isinstance(user, tuple) or user is None else user["id"]
    data = _read_json(handler)
    items = recommendation_service.recommend_personalized(
        user_id=user_id,
        preference_text=data.get("preference_text", ""),
        exclude_blacklist=bool(data.get("exclude_blacklist")),
        limit=int(data.get("limit", 5)),
    )
    return success({"list": items})


def recommend_feed(handler):
    user = _auth_user(handler, require_login=False)
    user_id = None if isinstance(user, tuple) or user is None else user["id"]
    data = _read_json(handler)
    preference_text = data.get("preference_text", "")
    category = data.get("category")
    canteen_id = int(data["canteen_id"]) if data.get("canteen_id") else None
    limit = int(data.get("limit", 5))
    seed = int(data.get("seed", 0))

    items = recommendation_service.recommend_feed(
        user_id=user_id,
        preference_text=preference_text,
        canteen_id=canteen_id,
        category=category,
        exclude_blacklist=True,
        limit=limit,
        seed=seed,
    )
    ai_result = llm_service.recommend_with_deepseek(
        preference_text=preference_text,
        category=category,
        candidates=items,
        user_context={"logged_in": bool(user_id), "category": category or "", "canteen_id": canteen_id},
    )

    picked_ids = ai_result["picked_ids"]
    if ai_result["enabled"] and picked_ids:
        picked_set = set(picked_ids)
        picked_order = {stall_id: index for index, stall_id in enumerate(picked_ids)}
        items = [item for item in items if item["stall_id"] in picked_set]
        items.sort(key=lambda item: picked_order.get(item["stall_id"], 999))
    else:
        items.sort(key=lambda item: (-item["match_score"], -item["review_count"], item["stall_id"]))

    return success(
        {
            "list": items,
            "ai_summary": ai_result["summary"],
            "ai_tips": ai_result["tips"],
            "ai_enabled": ai_result["enabled"],
            "ai_model": ai_result["model"],
            "ai_source": ai_result["source"],
        }
    )


def admin_delete_review(handler, id):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    item = admin_service.delete_review(int(id))
    return success(item) if item else (404, error_payload("not_found"))


def admin_create_stall(handler):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    return success(admin_service.create_stall_by_admin(_read_json(handler)))


def admin_update_stall(handler, id):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    item = admin_service.update_stall_by_admin(int(id), _read_json(handler))
    return success(item) if item else (404, error_payload("not_found"))


def admin_delete_stall(handler, id):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    item = admin_service.delete_stall_by_admin(int(id))
    return success({"result": "success", "stall": item}) if item else (404, error_payload("not_found"))


def admin_create_canteen(handler):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    return success(admin_service.create_canteen_by_admin(_read_json(handler)))


def admin_update_canteen(handler, id):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    item = admin_service.update_canteen_by_admin(int(id), _read_json(handler))
    return success(item) if item else (404, error_payload("not_found"))


def admin_list_users(handler):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    return success({"list": admin_service.list_users_by_admin()})


def admin_update_user_role(handler, id):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    target_id = int(id)
    if target_id == user["id"]:
        return 400, {"code": 4010, "message": "不能修改自己的权限", "data": None}
    role = int(_read_json(handler).get("role", 0))
    if role not in [0, 1]:
        return 400, {"code": 4010, "message": "角色只能是 0 或 1", "data": None}
    item = admin_service.update_user_role_by_admin(target_id, role)
    return success(item) if item else (404, error_payload("not_found"))


def admin_list_tags(handler):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    return success({"list": admin_service.list_tags_by_admin()})


def admin_create_tag(handler):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    return success(admin_service.create_tag_by_admin(_read_json(handler)))


def admin_update_tag(handler, id):
    user = _require_role(handler, 1)
    if isinstance(user, tuple):
        return (403 if user[1]["code"] == 4003 else 401), user[1]
    item = admin_service.update_tag_by_admin(int(id), _read_json(handler))
    return success(item) if item else (404, error_payload("not_found"))


ROUTES = [
    ("POST", r"/api/auth/register", register),
    ("POST", r"/api/auth/login", login),
    ("GET", r"/api/auth/me", get_me),
    ("POST", r"/api/auth/logout", logout),
    ("PUT", r"/api/auth/password", change_password),
    ("PUT", r"/api/users/me/profile", update_profile),
    ("GET", r"/api/canteens", list_canteens),
    ("GET", r"/api/canteens/(?P<id>\d+)", get_canteen),
    ("GET", r"/api/categories", list_categories),
    ("GET", r"/api/tags", list_tags),
    ("GET", r"/api/stalls", list_stalls),
    ("GET", r"/api/stalls/(?P<id>\d+)", get_stall),
    ("GET", r"/api/stalls/(?P<id>\d+)/reviews", list_stall_reviews),
    ("POST", r"/api/reviews", submit_review),
    ("GET", r"/api/rankings/score", score_ranking),
    ("GET", r"/api/rankings/hot", hot_ranking),
    ("GET", r"/api/rankings/latest", latest_ranking),
    ("GET", r"/api/users/me/reviews", list_my_reviews),
    ("PUT", r"/api/users/me/reviews/(?P<id>\d+)", update_my_review),
    ("DELETE", r"/api/users/me/reviews/(?P<id>\d+)", delete_my_review),
    ("POST", r"/api/users/me/favorites", add_favorite),
    ("DELETE", r"/api/users/me/favorites/(?P<stall_id>\d+)", delete_favorite),
    ("GET", r"/api/users/me/favorites", list_favorites),
    ("POST", r"/api/users/me/blacklist", add_blacklist),
    ("DELETE", r"/api/users/me/blacklist/(?P<stall_id>\d+)", delete_blacklist),
    ("GET", r"/api/users/me/blacklist", list_blacklist),
    ("GET", r"/api/users/me/history", list_history),
    ("POST", r"/api/users/me/history", add_history),
    ("GET", r"/api/recommendations/today", recommend_today),
    ("POST", r"/api/recommendations/personalized", recommend_personalized),
    ("POST", r"/api/recommendations/feed", recommend_feed),
    ("DELETE", r"/api/admin/reviews/(?P<id>\d+)", admin_delete_review),
    ("POST", r"/api/admin/stalls", admin_create_stall),
    ("PUT", r"/api/admin/stalls/(?P<id>\d+)", admin_update_stall),
    ("DELETE", r"/api/admin/stalls/(?P<id>\d+)", admin_delete_stall),
    ("POST", r"/api/admin/canteens", admin_create_canteen),
    ("PUT", r"/api/admin/canteens/(?P<id>\d+)", admin_update_canteen),
    ("GET", r"/api/admin/users", admin_list_users),
    ("PUT", r"/api/admin/users/(?P<id>\d+)/role", admin_update_user_role),
    ("GET", r"/api/admin/tags", admin_list_tags),
    ("POST", r"/api/admin/tags", admin_create_tag),
    ("PUT", r"/api/admin/tags/(?P<id>\d+)", admin_update_tag),
]


if __name__ == "__main__":
    init_db()
    server = ThreadingHTTPServer((HOST, PORT), AppHandler)
    print(f"Server running at http://{HOST}:{PORT}")
    server.serve_forever()

