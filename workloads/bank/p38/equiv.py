"""Equivalence check between before and after parse_content."""
import importlib.util


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


before = load('target_before.py', 'before_mod')
after = load('target_after.py', 'after_mod')

cases = [
    ('', False),
    ('', True),
    ('single line', False),
    ('single line', True),
    ('  hello ¶ world  ', False),
    ('line one\nline two\nline three', False),
    ('line one\nline two\nline three', True),
    ('  ¶ a  \n  b ¶ \n\n   \n c  ', False),
    ('  ¶ a  \n  b ¶ \n\n   \n c  ', True),
    ('one\ntwo', True),
    ('\n\n\n', False),
    ('alpha beta\n\ngamma   delta ¶\n   ', True),
]

ok = True
for content, rfl in cases:
    rb = before.parse_content(content, rfl)
    ra = after.parse_content(content, rfl)
    if rb != ra:
        ok = False
        print('MISMATCH for %r rfl=%r: before=%r after=%r' % (content, rfl, rb, ra))

print('EQUIV=' + ('yes' if ok else 'no'))
