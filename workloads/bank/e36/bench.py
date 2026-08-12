import sys
import time

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Ensure bfs exists and is callable
bfs = getattr(target, "bfs", None)
if not callable(bfs):
    print(f"SKIP=bfs not found or not callable")
    sys.exit(0)

# Build deterministic graph
n = 300  # number of nodes
G = [[1 if i != j else 0 for j in range(n)] for i in range(n)]
source = 0
sink = n - 1

# Number of iterations to target ~1 second
iterations = 2000

# Warm-up (optional, not timed)
for _ in range(10):
    parent = [-1] * n
    bfs(G, source, sink, parent)

start = time.perf_counter()
for _ in range(iterations):
    parent = [-1] * n
    bfs(G, source, sink, parent)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")
sys.exit(0)