ERRORS = {
    "success": (0, "success"),
    "not_login": (4001, "not_login"),
    "forbidden": (4003, "forbidden"),
    "not_found": (4004, "not_found"),
    "duplicate_review": (4009, "duplicate_review"),
    "bad_request": (4010, "bad_request"),
    "server_error": (5000, "server_error"),
}


def error_payload(name: str, data=None):
    code, message = ERRORS.get(name, ERRORS["server_error"])
    return {"code": code, "message": message, "data": data}
