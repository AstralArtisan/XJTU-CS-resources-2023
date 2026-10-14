from __future__ import annotations

import json
import os
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

DEEPSEEK_URL = "https://api.deepseek.com/v1/chat/completions"
DEFAULT_API_KEY = ""  # API 密钥留空；通过 DEEPSEEK_API_KEY 配置。
DEFAULT_MODEL = os.getenv("DEEPSEEK_MODEL", "deepseek-chat")


def _extract_json(text: str):
    text = (text or "").strip()
    if not text:
        return None
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        pass
    start = text.find("{")
    end = text.rfind("}")
    if start != -1 and end != -1 and end > start:
        try:
            return json.loads(text[start : end + 1])
        except json.JSONDecodeError:
            return None
    return None


def _fallback(candidates, message):
    return {
        "enabled": False,
        "source": "local",
        "model": DEFAULT_MODEL,
        "summary": message,
        "tips": "下面是根据评分和热度给你挑的，仅供参考。",
        "picked_ids": [item["stall_id"] for item in candidates[:3]],
    }


def recommend_with_deepseek(preference_text: str, category: str | None, candidates: list[dict], user_context: dict | None = None):
    """使用 DeepSeek API 根据用户偏好从候选窗口中推荐餐厅。"""
    api_key = os.getenv("DEEPSEEK_API_KEY", DEFAULT_API_KEY).strip()
    if not candidates:
        return _fallback([], "这个条件下没找到合适的窗口，换个筛选试试？")
    if not api_key:
        return _fallback(candidates, "AI 助手暂时不在线，先给你看看口碑不错的几家。")

    compact_candidates = [
        {
            "stall_id": item["stall_id"],
            "stall_name": item["stall_name"],
            "canteen_name": item["canteen_name"],
            "category": item["category"],
            "avg_rating": item["avg_rating"],
            "review_count": item["review_count"],
            "tags": item["tags"],
            "reason": item["reason"],
        }
        for item in candidates[:6]
    ]
    context_text = json.dumps(
        {
            "category": category or "",
            "preference_text": preference_text or "",
            "candidate_windows": compact_candidates,
            "user_context": user_context or {},
        },
        ensure_ascii=False,
    )

    system_prompt = (
        "你是西安交通大学食堂推荐助手。"
        "用户会给你偏好描述和候选窗口列表，你的核心任务是：\n"
        "1. 仔细理解用户的偏好，例如口味（清淡/辣/甜）、类型（面食/米饭/小吃）、场景等；\n"
        "2. 从候选窗口中挑选最符合偏好的，若某窗口明显与偏好矛盾（如用户要清淡却是麻辣），必须排除；\n"
        "3. 只从给定候选窗口中选，不得虚构；\n"
        "4. picked_ids 数量不限，符合偏好的就选，不符合的不选，不必凑满 3 个；若全不符合可返回空数组。\n"
        "输出规则：\n"
        "- summary 只说推荐了什么、为什么合适，不得提及不推荐的窗口或矛盾原因；\n"
        "- tips 给一句针对用户偏好的实用建议。\n"
        "请输出 JSON，格式为："
        '{"summary":"一到两句中文，只说推荐理由","tips":"一句建议","picked_ids":[...]}。'
    )

    body = {
        "model": DEFAULT_MODEL,
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": context_text},
        ],
        "temperature": 0.3,
    }
    payload = json.dumps(body, ensure_ascii=False).encode("utf-8")
    request = Request(
        DEEPSEEK_URL,
        data=payload,
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        },
        method="POST",
    )

    try:
        with urlopen(request, timeout=30) as response:
            data = json.loads(response.read().decode("utf-8"))
    except HTTPError:
        return _fallback(candidates, "网络开小差了，先看看这几家吧。")
    except URLError:
        return _fallback(candidates, "连不上推荐服务，先给你展示本地结果。")
    except Exception:
        return _fallback(candidates, "推荐服务出了点问题，先看看这几家口碑不错的。")

    content = (
        data.get("choices", [{}])[0]
        .get("message", {})
        .get("content", "")
    )
    parsed = _extract_json(content)
    if not parsed:
        return {
            "enabled": True,
            "source": "deepseek",
            "model": DEFAULT_MODEL,
            "summary": content or "给你找到了几家，具体看下面的推荐。",
            "tips": "可以换个描述再试试，说得越具体推荐越准。",
            "picked_ids": [item["stall_id"] for item in candidates[:3]],
        }

    picked_ids = [int(item) for item in parsed.get("picked_ids", []) if str(item).isdigit()]
    if not picked_ids:
        picked_ids = [item["stall_id"] for item in candidates[:3]]

    return {
        "enabled": True,
        "source": "deepseek",
        "model": DEFAULT_MODEL,
        "summary": parsed.get("summary") or "给你挑了几家，看看合不合口味。",
        "tips": parsed.get("tips") or "口味变了随时可以重新说，我再给你找找。",
        "picked_ids": picked_ids[:3],
    }


# 向后兼容别名
recommend_with_openrouter = recommend_with_deepseek
