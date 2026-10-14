from __future__ import annotations

import base64
import hashlib
import hmac
import os


def hash_password(password: str) -> str:
    salt = os.urandom(16)
    digest = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, 120000)
    return base64.b64encode(salt + digest).decode("ascii")


def verify_password(password: str, hashed_value: str) -> bool:
    raw = base64.b64decode(hashed_value.encode("ascii"))
    salt, digest = raw[:16], raw[16:]
    candidate = hashlib.pbkdf2_hmac("sha256", password.encode("utf-8"), salt, 120000)
    return hmac.compare_digest(candidate, digest)
