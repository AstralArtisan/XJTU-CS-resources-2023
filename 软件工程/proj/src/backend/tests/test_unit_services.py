"""
单元测试 - 业务服务层（黑盒测试）

测试对象：
  - backend.services.stall_service   窗口查询与筛选
  - backend.services.review_service  评论提交、更新、删除
  - backend.services.ranking_service 排行榜查询

测试方法：黑盒测试（只关注输入输出，不依赖内部实现）
对应课程 PPT 13.3 Unit Testing：接口测试、边界条件、等价类划分
"""
import pytest
from backend.database.db import get_connection
from backend.utils.password import hash_password
from backend.services import stall_service, review_service, ranking_service


# ─── 测试数据辅助函数 ───────────────────────────────────────

def _insert_canteen(name="测试食堂", location="校区A"):
    conn = get_connection()
    cur = conn.execute("INSERT INTO canteen (name, location) VALUES (?, ?)", (name, location))
    conn.commit()
    cid = cur.lastrowid
    conn.close()
    return cid


def _insert_stall(canteen_id, name="测试窗口", category="快餐", status=1):
    conn = get_connection()
    cur = conn.execute(
        "INSERT INTO stall (canteen_id, name, category, description, status) VALUES (?, ?, ?, ?, ?)",
        (canteen_id, name, category, "测试描述", status),
    )
    conn.commit()
    sid = cur.lastrowid
    conn.close()
    return sid


def _insert_user(student_id="test001", username="测试用户", role=0):
    conn = get_connection()
    cur = conn.execute(
        "INSERT INTO user (student_id, username, password_hash, role) VALUES (?, ?, ?, ?)",
        (student_id, username, hash_password("123456"), role),
    )
    conn.commit()
    uid = cur.lastrowid
    conn.close()
    return uid


# ─────────────────────────────────────────────
# TC-U-10 ~ TC-U-17  窗口查询（黑盒）
# ─────────────────────────────────────────────

class TestStallService:
    """窗口服务黑盒测试"""

    def test_query_all_stalls(self):
        """TC-U-10 无筛选条件时返回所有启用窗口"""
        cid = _insert_canteen()
        _insert_stall(cid, "窗口A")
        _insert_stall(cid, "窗口B")
        items, total = stall_service.query_stalls()
        assert total == 2
        assert len(items) == 2

    def test_disabled_stall_excluded(self):
        """TC-U-11 停用窗口（status=0）不出现在查询结果中"""
        cid = _insert_canteen()
        _insert_stall(cid, "启用窗口", status=1)
        _insert_stall(cid, "停用窗口", status=0)
        items, total = stall_service.query_stalls()
        assert total == 1
        assert items[0]["name"] == "启用窗口"

    def test_filter_by_canteen(self):
        """TC-U-12 按食堂筛选只返回该食堂的窗口"""
        cid1 = _insert_canteen("食堂1")
        cid2 = _insert_canteen("食堂2")
        _insert_stall(cid1, "食堂1窗口")
        _insert_stall(cid2, "食堂2窗口")
        items, total = stall_service.query_stalls(canteen_id=cid1)
        assert total == 1
        assert items[0]["name"] == "食堂1窗口"

    def test_filter_by_keyword(self):
        """TC-U-13 关键词搜索匹配窗口名称"""
        cid = _insert_canteen()
        _insert_stall(cid, "兰州拉面窗口")
        _insert_stall(cid, "黄焖鸡米饭")
        items, total = stall_service.query_stalls(keyword="拉面")
        assert total == 1
        assert "拉面" in items[0]["name"]

    def test_filter_by_category(self):
        """TC-U-14 按分类筛选"""
        cid = _insert_canteen()
        _insert_stall(cid, "面条窗口", category="面条")
        _insert_stall(cid, "快餐窗口", category="快餐")
        items, total = stall_service.query_stalls(category="面条")
        assert total == 1
        assert items[0]["category"] == "面条"

    def test_sort_by_score(self):
        """TC-U-15 按评分排序，高分在前"""
        cid = _insert_canteen()
        sid1 = _insert_stall(cid, "低分窗口")
        sid2 = _insert_stall(cid, "高分窗口")
        uid = _insert_user()
        review_service.create_or_update_review(uid, sid1, 2, "一般")
        uid2 = _insert_user("test002", "用户2")
        review_service.create_or_update_review(uid2, sid2, 5, "很好")
        items, _ = stall_service.query_stalls(sort_by="score")
        assert items[0]["name"] == "高分窗口"

    def test_get_stall_detail_exists(self):
        """TC-U-16 获取存在的窗口详情"""
        cid = _insert_canteen()
        sid = _insert_stall(cid, "详情窗口")
        detail = stall_service.get_stall_detail(sid)
        assert detail is not None
        assert detail["name"] == "详情窗口"

    def test_get_stall_detail_not_found(self):
        """TC-U-17 获取不存在的窗口返回 None（边界条件）"""
        result = stall_service.get_stall_detail(99999)
        assert result is None


# ─────────────────────────────────────────────
# TC-U-18 ~ TC-U-24  评论服务（黑盒）
# ─────────────────────────────────────────────

class TestReviewService:
    """评论服务黑盒测试"""

    def setup_method(self):
        self.cid = _insert_canteen()
        self.sid = _insert_stall(self.cid, "评论测试窗口")
        self.uid = _insert_user()

    def test_create_review(self):
        """TC-U-18 正常提交评论"""
        review = review_service.create_or_update_review(self.uid, self.sid, 5, "很好吃")
        assert review is not None
        assert review["rating"] == 5
        assert review["content"] == "很好吃"

    def test_update_existing_review(self):
        """TC-U-19 同一用户对同一窗口再次评论应更新而非新增"""
        review_service.create_or_update_review(self.uid, self.sid, 3, "一般")
        review_service.create_or_update_review(self.uid, self.sid, 5, "改变想法了，很好")
        items, total = review_service.get_reviews_of_stall(self.sid)
        assert total == 1
        assert items[0]["rating"] == 5

    def test_avg_rating_recalculated(self):
        """TC-U-20 提交评论后窗口平均分自动重算"""
        uid2 = _insert_user("test002", "用户2")
        review_service.create_or_update_review(self.uid, self.sid, 4, "不错")
        review_service.create_or_update_review(uid2, self.sid, 2, "一般")
        detail = stall_service.get_stall_detail(self.sid)
        assert detail["avg_rating"] == 3.0
        assert detail["review_count"] == 2

    def test_soft_delete_review(self):
        """TC-U-21 软删除评论后不再出现在列表中"""
        review = review_service.create_or_update_review(self.uid, self.sid, 5, "好")
        review_service.soft_delete_review(review["id"])
        items, total = review_service.get_reviews_of_stall(self.sid)
        assert total == 0

    def test_soft_delete_updates_stats(self):
        """TC-U-22 软删除后窗口统计数据同步更新"""
        review = review_service.create_or_update_review(self.uid, self.sid, 5, "好")
        review_service.soft_delete_review(review["id"])
        detail = stall_service.get_stall_detail(self.sid)
        assert detail["review_count"] == 0
        assert detail["avg_rating"] == 0

    def test_review_nonexistent_stall(self):
        """TC-U-23 对不存在的窗口评论返回 None（边界条件）"""
        result = review_service.create_or_update_review(self.uid, 99999, 5, "不存在")
        assert result is None

    def test_get_reviews_pagination(self):
        """TC-U-24 评论分页：page_size=1 只返回 1 条"""
        uid2 = _insert_user("test002", "用户2")
        review_service.create_or_update_review(self.uid, self.sid, 5, "评论1")
        review_service.create_or_update_review(uid2, self.sid, 4, "评论2")
        items, total = review_service.get_reviews_of_stall(self.sid, page=1, page_size=1)
        assert total == 2
        assert len(items) == 1


# ─────────────────────────────────────────────
# TC-U-25 ~ TC-U-28  排行榜服务（黑盒）
# ─────────────────────────────────────────────

class TestRankingService:
    """排行榜服务黑盒测试"""

    def setup_method(self):
        self.cid = _insert_canteen()
        self.sid1 = _insert_stall(self.cid, "高分窗口")
        self.sid2 = _insert_stall(self.cid, "热门窗口")
        self.sid3 = _insert_stall(self.cid, "新评窗口")
        uid1 = _insert_user("r001", "用户1")
        uid2 = _insert_user("r002", "用户2")
        uid3 = _insert_user("r003", "用户3")
        review_service.create_or_update_review(uid1, self.sid1, 5, "高分")
        review_service.create_or_update_review(uid2, self.sid2, 3, "热门1")
        review_service.create_or_update_review(uid3, self.sid2, 4, "热门2")
        review_service.create_or_update_review(uid1, self.sid3, 4, "最新")

    def test_score_ranking_order(self):
        """TC-U-25 评分榜：评分最高的窗口排第一"""
        result = ranking_service.query_score_ranking(limit=10)
        assert result[0]["stall_id"] == self.sid1

    def test_hot_ranking_order(self):
        """TC-U-26 热度榜：评论数最多的窗口排第一"""
        # sid2 有 2 条评论，sid1 有 1 条，sid3 有 1 条
        uid4 = _insert_user("r004", "用户4")
        review_service.create_or_update_review(uid4, self.sid2, 3, "热门额外评论")
        result = ranking_service.query_hot_ranking(limit=10)
        assert result[0]["stall_id"] == self.sid2

    def test_ranking_limit(self):
        """TC-U-27 limit 参数生效，最多返回指定数量"""
        result = ranking_service.query_score_ranking(limit=2)
        assert len(result) <= 2

    def test_disabled_stall_not_in_ranking(self):
        """TC-U-28 停用窗口不出现在排行榜中"""
        conn = get_connection()
        conn.execute("UPDATE stall SET status = 0 WHERE id = ?", (self.sid1,))
        conn.commit()
        conn.close()
        result = ranking_service.query_score_ranking(limit=10)
        ids = [r["stall_id"] for r in result]
        assert self.sid1 not in ids
