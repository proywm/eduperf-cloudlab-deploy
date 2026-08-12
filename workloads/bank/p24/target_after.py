"""Extracted _construct_response_bytes (after): encodes body only once."""


class Status:
    def __init__(self, code, text):
        self.code = code
        self.text = text


class Headers(dict):
    """Minimal dict-like headers (setdefault + items, like HTTPHeaders)."""


def _construct_response_bytes(
    http_version="HTTP/1.1",
    status=None,
    content_type="text/plain",
    content_length=None,
    headers=None,
    body="",
):
    """Constructs the response bytes from the given parameters."""

    response_message_header = f"{http_version} {status.code} {status.text}\r\n"
    encoded_response_message_body = body.encode("utf-8")

    headers.setdefault("Content-Type", content_type)
    headers.setdefault(
        "Content-Length", content_length or len(encoded_response_message_body)
    )
    headers.setdefault("Connection", "close")

    for header, value in headers.items():
        response_message_header += f"{header}: {value}\r\n"
    response_message_header += "\r\n"

    return response_message_header.encode("utf-8") + encoded_response_message_body
