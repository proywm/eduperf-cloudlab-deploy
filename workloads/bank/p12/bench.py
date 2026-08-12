import time
import random
import target


def build_input(seed=1234):
    rng = random.Random(seed)

    def make_nested(depth, width):
        if depth == 0:
            # leaf: deterministic non-dict values
            return rng.randint(0, 1000000)
        d = {}
        for i in range(width):
            key = "k{}_{}".format(depth, i)
            if rng.random() < 0.6:
                d[key] = make_nested(depth - 1, width)
            else:
                d[key] = rng.randint(0, 1000000)
        return d

    # Build a moderately deep & wide nested dict
    return {"root{}".format(i): make_nested(4, 4) for i in range(20)}


def main():
    base = build_input()
    N = 200

    # warmup
    target.flatten_dict(base)

    start = time.perf_counter()
    for _ in range(N):
        # pass a fresh shallow copy each iter so input dict structure is stable;
        # flatten_dict copies internally anyway.
        target.flatten_dict(base)
    end = time.perf_counter()

    print("TIME_SECONDS={}".format(end - start))


if __name__ == "__main__":
    main()
