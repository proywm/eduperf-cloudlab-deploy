from typing import List


class Variable:
    """Minimal stand-in for frosch.writer.Variable (only .name and .value used)."""
    def __init__(self, name, col_offset=0):
        self.name = name
        self.col_offset = col_offset
        self.value = None

    def __eq__(self, other):
        return (isinstance(other, Variable)
                and self.name == other.name
                and self.value == other.value)

    def __repr__(self):
        return f"Variable(name={self.name!r}, value={self.value!r})"


def debug_variables(variables: List[Variable], locals_: dict, globals_: dict) -> List[Variable]:
    """Evaluate for every given variable the value and type"""
    for var in variables:
        try:
            value = eval(var.name, globals_, locals_)
            var.value = value
        except NameError:
            pass
    return variables
