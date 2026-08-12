import sys
import time

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Ensure the function exists and is callable
if not hasattr(target, "_each_cons") or not callable(target._each_cons):
    print("SKIP=target._each_cons not found or not callable")
    sys.exit(0)

# Build deterministic input data
N = 200000          # length of the list
n = 3               # window size
iterations = 10     # number of times to call _each_cons

# Create a list of integers (deterministic)
xs = list(range(N))

# Warm-up (optional, not timed)
for _ in range(2):
    for _ in target._each_cons(xs, n):
        pass

# Timed loop
start = time.perf_counter()
for _ in range(iterations):
    for _ in target._each_cons(xs, n):
        pass
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed:.6f}")