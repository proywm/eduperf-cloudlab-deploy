_invalid_uri_chars = '<>" {}|\\^`'


def _is_valid_uri(uri):
    for c in uri:
        if c in _invalid_uri_chars:
            return False
    return True
