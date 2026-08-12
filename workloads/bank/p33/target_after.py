class Stream:
    """Minimal stand-in base class used only for isinstance checks."""
    pass


def input_streams(get_coefficient):
    closure = get_coefficient.__closure__
    if closure is None:
        return []
    return [cell.cell_contents for cell in closure
            if isinstance(cell.cell_contents, Stream)]
