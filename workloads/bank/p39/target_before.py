_invalid_uri_chars = '<>" {}|\\^`'


def _is_valid_uri(uri):
    return not bool([c for c in uri if c in _invalid_uri_chars and ord(c) <= 256])
