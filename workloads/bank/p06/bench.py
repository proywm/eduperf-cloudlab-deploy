import random
import time

import target


def make_names(n_files, depth, seed=1234):
    rng = random.Random(seed)
    # build a pool of directory components
    components = ['dir%d' % i for i in range(40)]
    names = []
    for _ in range(n_files):
        d = rng.randint(1, depth)
        parts = [rng.choice(components) for _ in range(d)]
        name = '/'.join(parts) + '/file%d.txt' % rng.randint(0, 10000)
        names.append(name)
    return names


def main():
    names = make_names(n_files=600, depth=5)
    iterations = 50

    # warmup
    list(target._implied_dirs(names))

    start = time.perf_counter()
    for _ in range(iterations):
        list(target._implied_dirs(names))
    elapsed = time.perf_counter() - start
    print("TIME_SECONDS=%.6f" % elapsed)


if __name__ == "__main__":
    main()
