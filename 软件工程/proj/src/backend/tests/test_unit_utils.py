"""
单元测试 - 工具函数层（白盒测试）

测试对象：
  - backend.utils.auth    JWT 令牌创建与解析
  - backend.utils.password 密码哈希与验证

测试方法：白盒测试（直接调用内部函数，验证逻辑分支）
对应课程 PPT 13.3 Unit Testing：接口、边界条件、独立路径
"""
import time
import pytest
from backend.utils.auth import create_token, parse_token
from backend.utils.password import hash_password, verify_password


# ─────────────────────────────────────────────
# TC-U-01 ~ TC-U-05  JWT 令牌
# ─────────────────────────────────────────────

class TestJWT:
    """JWT 令牌单元测试（白盒）"""

    def test_create_and_parse_normal(self):
        """TC-U-01 正常创建并解析令牌，payload 字段完整"""
        token = create_token(user_id=1, role=0)
        payload = parse_token(token)
        assert payload is not None
        assert payload["user_id"] == 1
        assert payload["role"] == 0
        assert payload["exp"] > int(time.time())

    def test_admin_role_preserved(self):
        """TC-U-02 管理员角色（role=1）在令牌中正确保留"""
        token = create_token(user_id=42, role=1)
        payload = parse_token(token)
        assert payload["role"] == 1
        assert payload["user_id"] == 42

    def test_tampered_token_rejected(self):
        """TC-U-03 篡改令牌签名后应返回 None（安全边界）"""
        token = create_token(user_id=1, role=0)
        tampered = token[:-4] + "XXXX"
        assert parse_token(tampered) is None

    def test_malformed_token_rejected(self):
        """TC-U-04 格式错误的令牌应返回 None"""
        assert parse_token("not.a.valid.token") is None
        assert parse_token("") is None
        assert parse_token("onlybody") is None

    def test_expired_token_rejected(self, monkeypatch):
        """TC-U-05 过期令牌应返回 None（边界条件：exp < now）"""
        import backend.utils.auth as auth_mod
        # 创建一个 exp 在过去的令牌
        original_time = time.time
        monkeypatch.setattr(time, "time", lambda: original_time() - 8 * 24 * 3600)
        token = create_token(user_id=1, role=0)
        monkeypatch.setattr(time, "time", original_time)
        assert parse_token(token) is None


# ─────────────────────────────────────────────
# TC-U-06 ~ TC-U-09  密码哈希
# ─────────────────────────────────────────────

class TestPassword:
    """密码哈希单元测试（白盒）"""

    def test_hash_is_not_plaintext(self):
        """TC-U-06 哈希结果不等于明文"""
        h = hash_password("123456")
        assert h != "123456"
        assert len(h) > 20

    def test_correct_password_verifies(self):
        """TC-U-07 正确密码验证通过"""
        h = hash_password("mypassword")
        assert verify_password("mypassword", h) is True

    def test_wrong_password_rejected(self):
        """TC-U-08 错误密码验证失败"""
        h = hash_password("mypassword")
        assert verify_password("wrongpassword", h) is False

    def test_same_password_different_hash(self):
        """TC-U-09 相同密码两次哈希结果不同（salt 机制）"""
        h1 = hash_password("samepassword")
        h2 = hash_password("samepassword")
        assert h1 != h2
        # 但两者都能验证通过
        assert verify_password("samepassword", h1)
        assert verify_password("samepassword", h2)
