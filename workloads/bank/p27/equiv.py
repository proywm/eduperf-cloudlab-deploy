import target_before as before
import target_after as after

methods = ['average', 'sum', 'last', 'max', 'min']

cases = {
    'single': [42.0],
    'typical': [1.0, 2.0, 3.0, 4.0, 5.0],
    'larger': [float(x) for x in range(1000)],
}

ok = True
for name, vals in cases.items():
    for m in methods:
        b = before.aggregate(m, list(vals))
        a = after.aggregate(m, list(vals))
        if b != a:
            ok = False
            print("MISMATCH method=%s case=%s before=%r after=%r" % (m, name, b, a))

# avg_zero needs neighborValues
nv = [1.0, None, 3.0, 0.0, 5.0]
b = before.aggregate('avg_zero', [], nv)
a = after.aggregate('avg_zero', [], nv)
if b != a:
    ok = False
    print("MISMATCH avg_zero before=%r after=%r" % (b, a))

# empty input for 'last' should raise in both identically
for label, fn in (('before', before.aggregate), ('after', after.aggregate)):
    try:
        fn('last', [])
        raised = False
    except IndexError:
        raised = True
    if not raised:
        ok = False
        print("empty 'last' did not raise in %s" % label)

print("EQUIV=" + ("yes" if ok else "no"))
