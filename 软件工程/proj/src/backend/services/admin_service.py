from __future__ import annotations

from backend.services.canteen_service import create_canteen, update_canteen
from backend.services.review_service import soft_delete_review
from backend.services.stall_service import create_stall, create_tag, disable_stall, list_tags, update_stall, update_tag
from backend.services.user_service import list_users, update_user_role


def delete_review(review_id: int):
    return soft_delete_review(review_id)


def create_stall_by_admin(data: dict):
    return create_stall(data)


def update_stall_by_admin(stall_id: int, data: dict):
    return update_stall(stall_id, data)


def delete_stall_by_admin(stall_id: int):
    return disable_stall(stall_id)


def create_canteen_by_admin(data: dict):
    return create_canteen(data)


def update_canteen_by_admin(canteen_id: int, data: dict):
    return update_canteen(canteen_id, data)


def list_users_by_admin():
    return list_users()


def update_user_role_by_admin(user_id: int, role: int):
    return update_user_role(user_id, role)


def list_tags_by_admin():
    return list_tags()


def create_tag_by_admin(data: dict):
    return create_tag(data)


def update_tag_by_admin(tag_id: int, data: dict):
    return update_tag(tag_id, data)
