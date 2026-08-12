import sys
import time

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Deterministic input: a Release with a large extra tuple
extra_tuple = tuple(range(2000))

# Number of iterations to target ~0.2-2 seconds
iterations = 5000

start = time.perf_counter()
for _ in range(iterations):
    _ = target.Release(major=1, minor=2, patch=3, extra=extra_tuple)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")