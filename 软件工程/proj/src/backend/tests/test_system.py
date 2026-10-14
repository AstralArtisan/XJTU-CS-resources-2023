"""
系统测试 - API 接口层（黑盒测试）

测试对象：HTTP API 端点的完整请求/响应
测试方法：黑盒测试，模拟真实 HTTP 请求，验证响应格式与业务规则
对应课程 PPT 13.5 Validation Testing + 13.6 System Testing
覆盖：接口完整性、功能有效性、安全性（未授权访问）
"""
import json
import sys
import os
import threading
import urllib.request
import urllib.error
import pytest

import pytest

import backend.database.db as db_module
from backend.database.db import init_db
from backend.main import AppHandler
from http.server import ThreadingHTTPServer

# 覆盖 conftest 的 autouse fixture，系统测试自己管理数据库
@pytest.fixture(autouse=True)
def use_temp_db():
    """系统测试不使用 conftest 的隔离数据库，由 server fixture 统一管理。"""
    yield


# ─── 测试服务器 fixture ──────────────────────────────────────

@pytest.fixture(scope="module")
def server(tmp_path_factory):
    """启动一个临时测试服务器，模块级共享，预置基础数据"""
    tmp = tmp_path_factory.mktemp("sysdb")
    db_path = tmp / "sys_test.sqlite3"
    db_module.DB_PATH = db_path
    init_db()

    # 预置一个食堂和窗口，供评论相关测试使用
    from backend.database.db import get_connection
    conn = get_connection()
    conn.execute("INSERT OR IGNORE INTO canteen (name, location) VALUES ('系统测试食堂', '测试校区')")
    conn.commit()
    cid = conn.execute("SELECT id FROM canteen WHERE name='系统测试食堂'").fetchone()["id"]
    conn.execute(
        "INSERT OR IGNORE INTO stall (canteen_id, name, category, description, status) VALUES (?, '系统测试窗口', '快餐', '测试用', 1)",
        (cid,)
    )
    conn.commit()
    conn.close()

    httpd = ThreadingHTTPServer(("127.0.0.1", 0), AppHandler)
    port = httpd.server_address[1]
    t = threading.Thread(target=httpd.serve_forever, daemon=True)
    t.start()
    yield f"http://127.0.0.1:{port}"
    httpd.shutdown()


def _req(base, path, method="GET", body=None, token=None):
    """发送 HTTP 请求，返回 (status_code, response_dict)"""
    url = base + path
    data = json.dumps(body).encode() if body else None
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.status, json.loads(resp.read())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read())


def _register_and_login(base, student_id="sys001", password="pass123", username="系统测试用户"):
    _req(base, "/api/auth/register", "POST", {"student_id": student_id, "username": username, "password": password})
    _, data = _req(base, "/api/auth/login", "POST", {"student_id": student_id, "password": password})
    return data["data"]["token"]


# ─────────────────────────────────────────────
# TC-S-01 ~ TC-S-04  认证接口
# ─────────────────────────────────────────────

class TestAuthAPI:
    """认证 API 系统测试"""

    def test_register_success(self, server):
        """TC-S-01 注册接口返回 code=0"""
        status, data = _req(server, "/api/auth/register", "POST", {
            "student_id": "s001", "username": "用户S1", "password": "pass"
        })
        assert data["code"] == 0

    def test_login_success_returns_token(self, server):
        """TC-S-02 登录成功返回 token 和 user 对象"""
        _req(server, "/api/auth/register", "POST", {"student_id": "s002", "username": "用户S2", "password": "pass"})
        status, data = _req(server, "/api/auth/login", "POST", {"student_id": "s002", "password": "pass"})
        assert data["code"] == 0
        assert "token" in data["data"]
        assert data["data"]["user"]["student_id"] == "s002"

    def test_login_wrong_password(self, server):
        """TC-S-03 密码错误登录返回错误码"""
        _req(server, "/api/auth/register", "POST", {"student_id": "s003", "username": "用户S3", "password": "right"})
        _, data = _req(server, "/api/auth/login", "POST", {"student_id": "s003", "password": "wrong"})
        assert data["code"] != 0

    def test_me_without_token(self, server):
        """TC-S-04 未携带 token 访问 /api/auth/me 返回 4001"""
        _, data = _req(server, "/api/auth/me")
        assert data["code"] == 4001


# ─────────────────────────────────────────────
# TC-S-05 ~ TC-S-09  窗口与搜索接口
# ─────────────────────────────────────────────

class TestStallAPI:
    """窗口 API 系统测试"""

    def test_list_stalls(self, server):
        """TC-S-05 获取窗口列表返回分页结构"""
        _, data = _req(server, "/api/stalls")
        assert data["code"] == 0
        assert "list" in data["data"]
        assert "total" in data["data"]

    def test_list_canteens(self, server):
        """TC-S-06 获取食堂列表"""
        _, data = _req(server, "/api/canteens")
        assert data["code"] == 0
        assert isinstance(data["data"]["list"], list)

    def test_stall_detail_not_found(self, server):
        """TC-S-07 查询不存在的窗口返回 4004"""
        _, data = _req(server, "/api/stalls/99999")
        assert data["code"] == 4004

    def test_search_by_keyword(self, server):
        """TC-S-08 关键词搜索接口正常响应"""
        import urllib.parse
        keyword = urllib.parse.quote("牛肉", safe="")
        _, data = _req(server, f"/api/stalls?keyword={keyword}")
        assert data["code"] == 0

    def test_filter_by_canteen(self, server):
        """TC-S-09 按食堂筛选接口正常响应"""
        _, data = _req(server, "/api/stalls?canteen_id=1")
        assert data["code"] == 0


# ─────────────────────────────────────────────
# TC-S-10 ~ TC-S-13  评论接口
# ─────────────────────────────────────────────

class TestReviewAPI:
    """评论 API 系统测试"""

    def test_submit_review_without_auth(self, server):
        """TC-S-10 未登录提交评论返回 4001（安全性测试）"""
        _, data = _req(server, "/api/reviews", "POST", {"stall_id": 1, "rating": 5, "content": "好"})
        assert data["code"] == 4001

    def test_submit_review_with_auth(self, server):
        """TC-S-11 登录后提交评论成功"""
        token = _register_and_login(server, "rev001", "pass", "评论用户")
        _, stalls = _req(server, "/api/stalls")
        if not stalls["data"]["list"]:
            pytest.skip("无可用窗口")
        stall_id = stalls["data"]["list"][0]["id"]
        _, data = _req(server, "/api/reviews", "POST",
                       {"stall_id": stall_id, "rating": 4, "content": "不错"},
                       token=token)
        assert data["code"] == 0

    def test_invalid_rating_rejected(self, server):
        """TC-S-12 评分超出 1-5 范围被拒绝（输入验证）"""
        token = _register_and_login(server, "rev002", "pass", "评分测试用户")
        _, stalls = _req(server, "/api/stalls")
        if not stalls["data"]["list"]:
            pytest.skip("无可用窗口")
        stall_id = stalls["data"]["list"][0]["id"]
        _, data = _req(server, "/api/reviews", "POST",
                       {"stall_id": stall_id, "rating": 6, "content": "超出范围"},
                       token=token)
        assert data["code"] != 0

    def test_get_stall_reviews(self, server):
        """TC-S-13 获取窗口评论列表"""
        _, stalls = _req(server, "/api/stalls")
        if not stalls["data"]["list"]:
            pytest.skip("无可用窗口")
        stall_id = stalls["data"]["list"][0]["id"]
        _, data = _req(server, f"/api/stalls/{stall_id}/reviews")
        assert data["code"] == 0
        assert "list" in data["data"]


# ─────────────────────────────────────────────
# TC-S-14 ~ TC-S-16  排行榜接口
# ─────────────────────────────────────────────

class TestRankingAPI:
    """排行榜 API 系统测试"""

    def test_score_ranking(self, server):
        """TC-S-14 评分榜接口正常响应"""
        _, data = _req(server, "/api/rankings/score")
        assert data["code"] == 0
        assert "list" in data["data"]

    def test_hot_ranking(self, server):
        """TC-S-15 热度榜接口正常响应"""
        _, data = _req(server, "/api/rankings/hot")
        assert data["code"] == 0

    def test_latest_ranking(self, server):
        """TC-S-16 最新评价榜接口正常响应"""
        _, data = _req(server, "/api/rankings/latest")
        assert data["code"] == 0


# ─────────────────────────────────────────────
# TC-S-17 ~ TC-S-19  权限控制（安全性测试）
# ─────────────────────────────────────────────

class TestSecurityAPI:
    """API 权限安全测试"""

    def test_admin_endpoint_requires_auth(self, server):
        """TC-S-17 管理员接口未登录返回 4001"""
        _, data = _req(server, "/api/admin/stalls", "POST", {"name": "非法窗口"})
        assert data["code"] == 4001

    def test_admin_endpoint_requires_admin_role(self, server):
        """TC-S-18 普通用户访问管理员接口返回 4003"""
        token = _register_and_login(server, "normal001", "pass", "普通用户")
        _, data = _req(server, "/api/admin/stalls", "POST",
                       {"canteen_id": 1, "name": "越权窗口", "category": "快餐"},
                       token=token)
        assert data["code"] == 4003

    def test_favorites_requires_auth(self, server):
        """TC-S-19 收藏接口未登录返回 4001"""
        _, data = _req(server, "/api/users/me/favorites", "POST", {"stall_id": 1})
        assert data["code"] == 4001
