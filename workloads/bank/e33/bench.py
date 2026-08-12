import sys
import time
import random

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# deterministic input
random.seed(0)
N = 2000  # size of list; adjust if needed for timing
base = [random.randint(0, 1000000) for _ in range(N)]

iterations = 1
elapsed = 0.0
while True:
    # build fresh copies outside the timed region
    pre_copies = [base.copy() for _ in range(iterations)]
    start = time.perf_counter()
    for seq in pre_copies:
        target.sort(seq)
    elapsed = time.perf_counter() - start
    if elapsed >= 0.2:
        break
    iterations *= 2
    if elapsed > 2:
        break

print(f"TIME_SECONDS={elapsed}")