#!/usr/bin/env python3
import sys
import time

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

bytesify = getattr(target, "bytesify", None)
if not callable(bytesify):
    print("SKIP=bytesify not found")
    sys.exit(0)

# Deterministic input data: a bytes object of moderate size
data = b'a' * 1000

# Number of iterations to make the benchmark run ~0.2-2 seconds
iterations = 10_000_000

start = time.perf_counter()
for _ in range(iterations):
    bytesify(data)
end = time.perf_counter()

print(f"TIME_SECONDS={end - start}")