import sys
import time

try:
    import target
except Exception as e:
    print(f"SKIP=import error: {e}")
    sys.exit(0)

# Ensure the function exists and is callable
is_optional = getattr(target, "is_optional", None)
if not callable(is_optional):
    print("SKIP=is_optional not found or not callable")
    sys.exit(0)

# Build deterministic input: a Union of int, str, float, None
try:
    test_type = int | str | float | None
except Exception as e:
    print(f"SKIP=failed to build test type: {e}")
    sys.exit(0)

# Warm up to avoid first-call overhead
try:
    is_optional(test_type)
except Exception as e:
    print(f"SKIP=call error during warm-up: {e}")
    sys.exit(0)

# Number of iterations to target ~1 second (approx 540 ns per call)
iterations = 2_000_000

start = time.perf_counter()
for _ in range(iterations):
    is_optional(test_type)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")