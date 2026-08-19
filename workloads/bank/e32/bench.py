import sys
import time
import random
import os

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Build deterministic input data
random.seed(0)
PROFILE = os.environ.get("EDUPERF_PROFILE") == "1"
N = 100_000 if PROFILE else 5_000_000  # large enough to dominate runtime
numbers = [random.random() * 100 for _ in range(N)]

# Warm‑up (optional, not timed)
try:
    target._create(numbers)
except AttributeError:
    print("SKIP=_create not found")
    sys.exit(0)

# Benchmark
loops = 3 if PROFILE else 10  # keep Python-frame profiling bounded
start = time.perf_counter()
for _ in range(loops):
    target._create(numbers)
end = time.perf_counter()

print(f"TIME_SECONDS={end - start}")
