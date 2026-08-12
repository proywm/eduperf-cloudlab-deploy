import time
import sys

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

def build_tree(depth, children_per_node):
    if depth == 0:
        return target.Node()
    return target.Node(children=[build_tree(depth - 1, children_per_node) for _ in range(children_per_node)])

# Build a deterministic tree: depth 8, 5 children per node → ~488k nodes
root = build_tree(8, 5)

# Warm‑up (outside timed region)
for _ in root.get_breadth_first_iterator():
    pass

start = time.perf_counter()
for _ in range(2):  # iterate twice to reach ~0.4–0.6 s
    for _ in root.get_breadth_first_iterator():
        pass
end = time.perf_counter()

print(f"TIME_SECONDS={end - start}")