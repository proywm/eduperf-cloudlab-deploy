from target_before import declaration_path as dp_before
from target_after import declaration_path as dp_after
from _decl import build_tree, build_chain, reset_cache, Decl


def run_one(fn, nodes):
    """Run fn over all nodes (root-first), return list of (path) results and the
    cached path on each node afterward (to compare mutation)."""
    reset_cache(nodes)
    results = []
    for d in nodes:
        results.append(list(fn(d)))
    caches = [list(d.cache.declaration_path) if d.cache.declaration_path else None
              for d in nodes]
    return results, caches


def make_cases():
    cases = {}
    # empty: None input
    cases["none"] = None
    # single node (no parent)
    cases["single"] = [Decl("solo")]
    # typical: a small tree
    cases["typical_tree"] = build_tree(5, 2, seed=7)
    # a deep chain
    cases["chain"] = build_chain(20, seed=3)
    # larger tree
    cases["larger_tree"] = build_tree(8, 3, seed=99)
    return cases


ok = True

# None case handled separately (both should return [])
if dp_before(None) != dp_after(None):
    ok = False

cases = make_cases()
for name, nodes in cases.items():
    if nodes is None:
        continue
    r_before, c_before = run_one(dp_before, nodes)
    r_after, c_after = run_one(dp_after, nodes)
    if r_before != r_after:
        print("MISMATCH return in case %s" % name)
        ok = False
    if c_before != c_after:
        print("MISMATCH cache/mutation in case %s" % name)
        ok = False

print("EQUIV=%s" % ("yes" if ok else "no"))
