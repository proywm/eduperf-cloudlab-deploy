"""Benchmark for EUI.eui64. Imports `target` (copy of before/after)."""
import random
import time

import target


def main():
    random.seed(1234)
    # Deterministic representative input: many distinct EUI-48 (MAC) addresses.
    macs = [random.randint(0, 0xffffffffffff) for _ in range(2000)]
    euis = [target.EUI(v, version=48) for v in macs]

    iterations = 60
    start = time.perf_counter()
    sink = 0
    for _ in range(iterations):
        for e in euis:
            sink ^= e.eui64()._value
    elapsed = time.perf_counter() - start

    # prevent dead-code elimination
    if sink == -1:
        print("impossible")
    print("TIME_SECONDS=%.6f" % elapsed)


if __name__ == "__main__":
    main()
