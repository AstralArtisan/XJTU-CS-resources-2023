from __future__ import annotations

import base64
import hashlib
import hmac
import json
import os
import time

TOKEN_SECRET = os.environ.get("CANTEEN_TOKEN_SECRET", "")  # 签名密钥留空；通过 CANTEEN_TOKEN_SECRET 配置。
TOKEN_EXPIRE_SECONDS = 7 * 24 * 60 * 60


def _urlsafe_encode(raw: bytes) -> str:
    return base64.urlsafe_b64encode(raw).decode("ascii").rstrip("=")


def _urlsafe_decode(value: str) -> bytes:
    padding = "=" * ((4 - len(value) % 4) % 4)
    return base64.urlsafe_b64decode((value + padding).encode("ascii"))


def create_token(user_id: int, role: int) -> str:
    payload = {"user_id": user_id, "role": role, "exp": int(time.time()) + TOKEN_EXPIRE_SECONDS}
    payload_bytes = json.dumps(payload, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    body = _urlsafe_encode(payload_bytes)
    signature = hmac.new(TOKEN_SECRET.encode("utf-8"), body.encode("ascii"), hashlib.sha256).digest()
    return f"{body}.{_urlsafe_encode(signature)}"


def parse_token(token: str):
    try:
        body, signature = token.split(".", 1)
        expected = hmac.new(TOKEN_SECRET.encode("utf-8"), body.encode("ascii"), hashlib.sha256).digest()
        if not hmac.compare_digest(expected, _urlsafe_decode(signature)):
            return None
        payload = json.loads(_urlsafe_decode(body).decode("utf-8"))
        if payload["exp"] < int(time.time()):
            return None
        return payload
    except Exception:
        return None
