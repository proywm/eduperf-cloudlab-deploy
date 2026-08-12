import random
import time

import target


def make_input(seed, n):
    rnd = random.Random(seed)
    # many duplicates so the dedup work dominates
    return [rnd.randint(0, n // 4) for _ in range(n)]


def main():
    data = make_input(1234, 20000)
    # with a hashfunc too, to exercise both branches
    iters = 200

    start = time.perf_counter()
    for _ in range(iters):
        target.unique_list(data)
        target.unique_list(data, hashfunc=lambda x: x % 1000)
    elapsed = time.perf_counter() - start
    print("TIME_SECONDS=%f" % elapsed)


if __name__ == "__main__":
    main()
