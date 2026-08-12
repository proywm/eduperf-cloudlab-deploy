def _split_comma_separated(string):
    """Return a set of strings."""
    return {text.strip() for text in string.split(',') if text.strip()}
