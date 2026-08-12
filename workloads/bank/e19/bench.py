import sys
import time
import random

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Ensure bubble_sort exists and is callable
if not hasattr(target, "bubble_sort") or not callable(target.bubble_sort):
    print("SKIP=target.bubble_sort not found or not callable")
    sys.exit(0)

# Build deterministic input data
random.seed(0)
SIZE = 5000
base_data = [random.randint(0, 1000) for _ in range(SIZE)]

# Warm‑up and single‑run timing
single_start = time.perf_counter()
target.bubble_sort(base_data.copy())
single_end = time.perf_counter()
single_time = single_end - single_start

# Guard against zero timing
if single_time <= 0:
    single_time = 1e-9

# Determine number of iterations to keep total time between 0.2 and 2 seconds
if single_time > 2:
    iterations = 1
else:
    min_iters = max(1, int(0.2 / single_time))
    max_iters = max(1, int(2 / single_time))
    iterations = min(min_iters, max_iters)

# Timed loop
start = time.perf_counter()
for _ in range(iterations):
    data = base_data.copy()
    target.bubble_sort(data)
elapsed = time.perf_counter() - start

print(f"TIME_SECONDS={elapsed}")