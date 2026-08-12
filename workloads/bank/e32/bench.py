import sys
import time
import random

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Build deterministic input data
random.seed(0)
N = 5_000_000  # large enough to dominate runtime
numbers = [random.random() * 100 for _ in range(N)]

# Warm‑up (optional, not timed)
try:
    target._create(numbers)
except AttributeError:
    print("SKIP=_create not found")
    sys.exit(0)

# Benchmark
loops = 10  # adjust to hit ~0.2‑2 seconds
start = time.perf_counter()
for _ in range(loops):
    target._create(numbers)
end = time.perf_counter()

print(f"TIME_SECONDS={end - start}")