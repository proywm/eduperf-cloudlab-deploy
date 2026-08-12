import random
import time

import target


def make_blocks():
    rng = random.Random(1234)
    blocks = []
    # Mix of block "entropies": uniform random, skewed, and small alphabet.
    for _ in range(40):
        blocks.append(''.join(chr(rng.randint(0, 255)) for _ in range(1024)))
    for _ in range(40):
        # skewed toward a small set of bytes
        blocks.append(''.join(chr(rng.choice([0, 1, 2, 65, 200])) for _ in range(1024)))
    for _ in range(40):
        # mid-range alphabet
        blocks.append(''.join(chr(rng.randint(0, 63)) for _ in range(1024)))
    return blocks


def main():
    blocks = make_blocks()

    start = time.perf_counter()
    total = 0.0
    for _ in range(20):
        for b in blocks:
            total += target.shannon(b)
    elapsed = time.perf_counter() - start

    # keep total alive so work isn't optimized away
    if total < 0:
        print(total)
    print("TIME_SECONDS=%f" % elapsed)


if __name__ == '__main__':
    main()
