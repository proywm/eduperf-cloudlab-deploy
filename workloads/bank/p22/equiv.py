import importlib.util


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


before = load("target_before.py", "target_before")
after = load("target_after.py", "target_after")

# SAME inputs across both versions. Caches are per-class, independent.
string_inputs = [
    'A', 'B', 'Z', '<alt-l>', '<ctrl-x>', 'meta g', 'esc', 'pos1', 'del',
    'ins', 'return', 'page up', 'page down', 'space', 'j', 'k', 'enter',
    'shift-E', '<F1>', 'home', '-', 'a',
]

ok = True

# 1) string inputs: compare resulting normalized value + repr
for s in string_inputs:
    b = before.Key(s)
    a = after.Key(s)
    if str(b) != str(a) or repr(b) != repr(a) or type(b).__name__ != type(a).__name__:
        ok = False
        print("MISMATCH string %r: before=%r after=%r" % (s, str(b), str(a)))

# 2) re-wrapping a Key instance (the optimized path): result must be equal value
for s in string_inputs:
    b1 = before.Key(s)
    b2 = before.Key(b1)
    a1 = after.Key(s)
    a2 = after.Key(a1)
    if str(b2) != str(a2):
        ok = False
        print("MISMATCH rewrap %r: before=%r after=%r" % (s, str(b2), str(a2)))
    # both must round-trip to the same normalized value as single-wrap
    if str(b2) != str(b1) or str(a2) != str(a1):
        ok = False
        print("ROUNDTRIP CHANGED %r" % s)

# 3) error cases must match
for bad in ['ctrl-', 'bogus-x']:
    be = ae = None
    try:
        before.Key(bad)
    except Exception as e:
        be = type(e).__name__
    try:
        after.Key(bad)
    except Exception as e:
        ae = type(e).__name__
    if be != ae:
        ok = False
        print("MISMATCH error %r: before=%r after=%r" % (bad, be, ae))

# 4) empty-ish / single typical / larger sets already covered above.
print("EQUIV=" + ("yes" if ok else "no"))
