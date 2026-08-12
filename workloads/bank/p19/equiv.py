import importlib.util


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


before = load("target_before.py", "target_before")
after = load("target_after.py", "target_after")

# empty/single/typical/larger style cases (all valid OUI ints in [0, 0xffffff])
cases = [
    0,
    0x000001,
    0xffffff,
    0x0050c2,
    0xacde48,
    0x123456,
    0x00000a,
    0xff0000,
]

ok = True
for c in cases:
    rb = before.oui_str(c)
    ra = after.oui_str(c)
    if rb != ra:
        ok = False
        print("MISMATCH input=%r before=%r after=%r" % (c, rb, ra))

print("EQUIV=" + ("yes" if ok else "no"))
