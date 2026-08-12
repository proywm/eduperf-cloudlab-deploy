import target_before
import target_after


def run(mod, names, locals_, globals_):
    vs = [mod.Variable(nm, i) for i, nm in enumerate(names)]
    out = mod.debug_variables(vs, dict(locals_), dict(globals_))
    return [(v.name, v.value) for v in out], [(v.name, v.value) for v in vs]


cases = [
    # empty
    ([], {}, {}),
    # single resolving local
    (["x"], {"x": 42}, {}),
    # typical: mix of local, global, missing
    (["a", "b", "missing"], {"a": 1}, {"b": [1, 2, 3]}),
    # local shadows global (eval uses locals first; chainmap locals first)
    (["s"], {"s": "local"}, {"s": "global"}),
    # larger
    ([f"n{i}" for i in range(50)],
     {f"n{i}": i for i in range(0, 50, 2)},
     {f"n{i}": i * 10 for i in range(1, 50, 2)}),
]

ok = True
for names, locals_, globals_ in cases:
    ret_b, mut_b = run(target_before, names, locals_, globals_)
    ret_a, mut_a = run(target_after, names, locals_, globals_)
    if ret_b != ret_a or mut_b != mut_a:
        ok = False
        print(f"MISMATCH names={names}")
        print(f"  before ret={ret_b}")
        print(f"  after  ret={ret_a}")

print(f"EQUIV={'yes' if ok else 'no'}")
