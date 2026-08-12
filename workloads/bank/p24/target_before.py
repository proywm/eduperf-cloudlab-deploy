"""Extracted _construct_response_bytes (before): encodes body twice."""


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

    response = f"{http_version} {status.code} {status.text}\r\n"

    headers.setdefault("Content-Type", content_type)
    headers.setdefault("Content-Length", content_length or len(body.encode("utf-8")))
    headers.setdefault("Connection", "close")

    for header, value in headers.items():
        response += f"{header}: {value}\r\n"

    response += f"\r\n{body}"

    return response.encode("utf-8")
