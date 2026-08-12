import time
import target
from _decl import build_tree, reset_cache

# Deterministic representative input: a moderately deep, branching declaration
# tree (like nested namespaces/classes). Root-first iteration means each node's
# parent path is already cached -- the scenario the optimization targets.
DEPTH = 14
BRANCHING = 2
nodes = build_tree(DEPTH, BRANCHING, seed=12345)  # ~16k nodes

N = 40  # repeat the full traversal N times

best = None
# warm one traversal happens inside the loop each iter after reset
start = time.perf_counter()
for _ in range(N):
    reset_cache(nodes)
    for d in nodes:  # root-first => parent caches populated before children
        target.declaration_path(d)
elapsed = time.perf_counter() - start

print("TIME_SECONDS=%r" % (elapsed,))
