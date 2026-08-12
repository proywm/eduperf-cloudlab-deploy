import sys
import time
import random

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Ensure the Node class exists
if not hasattr(target, "Node"):
    print("SKIP=target.Node not found")
    sys.exit(0)

# Deterministic input data
random.seed(0)
rPath = [f"node{i}" for i in range(1000)]
results = set(range(1000))

# Number of iterations to target ~0.2-2 seconds
iterations = 2000000

start = time.perf_counter()
for _ in range(iterations):
    target.Node(rPath, results)
end = time.perf_counter()

print(f"TIME_SECONDS={end - start}")