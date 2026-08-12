#!/usr/bin/env python3
import sys
try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

import time

def main():
    # Deterministic keyword arguments for Model.__init__
    keys = [f'k{i}' for i in range(10)]
    base_kwargs = {k: i for i, k in enumerate(keys)}
    # Number of iterations to target ~0.2-2 seconds
    n = 200000
    start = time.perf_counter()
    for _ in range(n):
        target.Model(**base_kwargs)
    elapsed = time.perf_counter() - start
    print(f"TIME_SECONDS={elapsed}")

if __name__ == "__main__":
    main()