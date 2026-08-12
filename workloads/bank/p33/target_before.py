class Stream:
    """Minimal stand-in base class used only for isinstance checks."""
    pass


def input_streams(get_coefficient):
    closure = get_coefficient.__closure__
    if closure is None:
        return []
    l = []
    for cell in closure:
        content = cell.cell_contents
        if isinstance(content, Stream):
            l.append(content)
    return l
