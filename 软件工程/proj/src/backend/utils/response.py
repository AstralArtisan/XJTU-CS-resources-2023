from __future__ import annotations


def success(data=None, message: str = "success"):
    return {"code": 0, "message": message, "data": {} if data is None else data}


def page_success(items, total: int, page: int, page_size: int, extra: dict | None = None):
    payload = {
        "list": items,
        "total": total,
        "page": page,
        "page_size": page_size,
    }
    if extra:
        payload.update(extra)
    return success(payload)
