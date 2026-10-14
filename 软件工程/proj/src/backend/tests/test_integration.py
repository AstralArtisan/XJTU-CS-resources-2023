"""
集成测试 - 业务流程层

测试对象：跨服务的完整业务流程
  - 用户注册 → 登录 → 评论 → 排行榜反映
  - 收藏 / 黑名单 / 历史记录流程
  - 管理员操作流程（停用窗口、删除评论）

测试方法：集成测试（多个模块协同工作，验证数据流）
对应课程 PPT 13.3 Integration Testing：自顶向下集成
"""
import pytest
from backend.database.db import get_connection
from backend.utils.password import hash_password
from backend.services import (
    auth_service,
    stall_service,
    review_service,
    ranking_service,
    user_service,
)


def _insert_user(student_id, username, role=0):
    conn = get_connection()
    from backend.utils.password import hash_password as hp
    cur = conn.execute(
        "INSERT INTO user (student_id, username, password_hash, role) VALUES (?, ?, ?, ?)",
        (student_id, username, hp("123456"), role),
    )
    conn.commit()
    uid = cur.lastrowid
    conn.close()
    return uid


# ─── 辅助函数 ────────────────────────────────────────────────

def _setup_base():
    """创建基础数据：1个食堂、2个窗口、2个用户"""
    conn = get_connection()
    conn.execute("INSERT INTO canteen (name, location) VALUES ('集成食堂', '校区B')")
    conn.commit()
    cid = conn.execute("SELECT id FROM canteen WHERE name='集成食堂'").fetchone()["id"]
    conn.execute("INSERT INTO stall (canteen_id, name, category, description, status) VALUES (?, '集成窗口A', '快餐', '描述A', 1)", (cid,))
    conn.execute("INSERT INTO stall (canteen_id, name, category, description, status) VALUES (?, '集成窗口B', '面条', '描述B', 1)", (cid,))
    conn.commit()
    sid_a = conn.execute("SELECT id FROM stall WHERE name='集成窗口A'").fetchone()["id"]
    sid_b = conn.execute("SELECT id FROM stall WHERE name='集成窗口B'").fetchone()["id"]
    conn.close()
    uid1 = _insert_user("it001", "集成用户1")
    uid2 = _insert_user("it002", "集成用户2")
    return cid, sid_a, sid_b, uid1, uid2


# ─────────────────────────────────────────────
# TC-I-01 ~ TC-I-04  用户注册登录流程
# ─────────────────────────────────────────────

class TestAuthFlow:
    """用户认证集成测试"""

    def test_register_and_login(self):
        """TC-I-01 注册后可以正常登录，返回 token"""
        user, err = auth_service.register_user("flow001", "流程用户", "mypass")
        assert user is not None
        result = auth_service.login_user("flow001", "mypass")
        assert result is not None
        assert "token" in result
        assert result["user"]["student_id"] == "flow001"

    def test_duplicate_register_fails(self):
        """TC-I-02 重复注册同一学号应失败"""
        auth_service.register_user("dup001", "用户A", "pass")
        user, err = auth_service.register_user("dup001", "用户B", "pass")
        assert user is None
        assert err is not None

    def test_wrong_password_login_fails(self):
        """TC-I-03 密码错误登录失败"""
        auth_service.register_user("pw001", "密码测试", "correct")
        result = auth_service.login_user("pw001", "wrong")
        assert result is None

    def test_nonexistent_user_login_fails(self):
        """TC-I-04 不存在的用户登录失败"""
        result = auth_service.login_user("nobody999", "pass")
        assert result is None


# ─────────────────────────────────────────────
# TC-I-05 ~ TC-I-08  评论与排行榜联动
# ─────────────────────────────────────────────

class TestReviewRankingIntegration:
    """评论提交后排行榜数据联动测试"""

    def setup_method(self):
        _, self.sid_a, self.sid_b, self.uid1, self.uid2 = _setup_base()

    def test_review_affects_score_ranking(self):
        """TC-I-05 提交高分评论后，该窗口出现在评分榜前列"""
        review_service.create_or_update_review(self.uid1, self.sid_a, 5, "非常好")
        review_service.create_or_update_review(self.uid2, self.sid_b, 2, "一般")
        ranking = ranking_service.query_score_ranking(limit=10)
        top_id = ranking[0]["stall_id"]
        assert top_id == self.sid_a

    def test_review_affects_hot_ranking(self):
        """TC-I-06 评论数多的窗口在热度榜排前"""
        # sid_b 获得 2 条评论，sid_a 只有 1 条
        review_service.create_or_update_review(self.uid1, self.sid_b, 4, "评论1")
        review_service.create_or_update_review(self.uid2, self.sid_a, 5, "评论2")
        uid_extra = _insert_user("extra01", "额外用户")
        review_service.create_or_update_review(uid_extra, self.sid_b, 3, "评论3")
        ranking = ranking_service.query_hot_ranking(limit=10)
        assert ranking[0]["stall_id"] == self.sid_b

    def test_delete_review_updates_ranking(self):
        """TC-I-07 删除评论后排行榜数据同步更新"""
        r = review_service.create_or_update_review(self.uid1, self.sid_a, 5, "好")
        review_service.create_or_update_review(self.uid2, self.sid_b, 3, "一般")
        review_service.soft_delete_review(r["id"])
        detail = stall_service.get_stall_detail(self.sid_a)
        assert detail["review_count"] == 0
        assert detail["avg_rating"] == 0.0

    def test_update_review_recalculates_avg(self):
        """TC-I-08 修改评分后平均分重新计算"""
        review_service.create_or_update_review(self.uid1, self.sid_a, 2, "一般")
        review_service.create_or_update_review(self.uid1, self.sid_a, 5, "改变想法")
        detail = stall_service.get_stall_detail(self.sid_a)
        assert detail["avg_rating"] == 5.0


# ─────────────────────────────────────────────
# TC-I-09 ~ TC-I-12  收藏 / 黑名单 / 历史
# ─────────────────────────────────────────────

class TestUserBehaviorIntegration:
    """用户行为（收藏、黑名单、历史）集成测试"""

    def setup_method(self):
        _, self.sid_a, self.sid_b, self.uid1, _ = _setup_base()

    def test_add_and_list_favorite(self):
        """TC-I-09 收藏窗口后出现在收藏列表"""
        user_service.add_favorite(self.uid1, self.sid_a)
        items, total = user_service.list_favorites(self.uid1, 1, 10)
        assert total == 1
        assert items[0]["stall_id"] == self.sid_a

    def test_remove_favorite(self):
        """TC-I-10 取消收藏后从列表移除"""
        user_service.add_favorite(self.uid1, self.sid_a)
        user_service.remove_favorite(self.uid1, self.sid_a)
        _, total = user_service.list_favorites(self.uid1, 1, 10)
        assert total == 0

    def test_blacklist_add_and_list(self):
        """TC-I-11 拉黑窗口后出现在黑名单"""
        user_service.add_blacklist(self.uid1, self.sid_b)
        items, total = user_service.list_blacklist(self.uid1, 1, 10)
        assert total == 1
        assert items[0]["stall_id"] == self.sid_b

    def test_history_dedup_within_30min(self):
        """TC-I-12 30分钟内重复访问同一窗口只记录一次"""
        user_service.add_history(self.uid1, self.sid_a)
        user_service.add_history(self.uid1, self.sid_a)
        _, total = user_service.list_history(self.uid1, 1, 10)
        assert total == 1


# ─────────────────────────────────────────────
# TC-I-13 ~ TC-I-15  管理员操作流程
# ─────────────────────────────────────────────

class TestAdminFlow:
    """管理员操作集成测试"""

    def setup_method(self):
        self.cid, self.sid_a, _, self.uid1, _ = _setup_base()

    def test_disable_stall_hides_from_search(self):
        """TC-I-13 管理员停用窗口后搜索结果中不再出现"""
        conn = get_connection()
        conn.execute("UPDATE stall SET status = 0 WHERE id = ?", (self.sid_a,))
        conn.commit()
        conn.close()
        items, total = stall_service.query_stalls()
        ids = [i["id"] for i in items]
        assert self.sid_a not in ids

    def test_admin_delete_review(self):
        """TC-I-14 管理员软删除评论后评论不可见"""
        r = review_service.create_or_update_review(self.uid1, self.sid_a, 5, "好评")
        review_service.soft_delete_review(r["id"])
        items, total = review_service.get_reviews_of_stall(self.sid_a)
        assert total == 0

    def test_create_stall_by_admin(self):
        """TC-I-15 管理员新增窗口后可以查询到"""
        new_stall = stall_service.create_stall({
            "canteen_id": self.cid,
            "name": "新增测试窗口",
            "category": "小吃",
            "description": "管理员新增",
        })
        assert new_stall is not None
        detail = stall_service.get_stall_detail(new_stall["id"])
        assert detail["name"] == "新增测试窗口"
