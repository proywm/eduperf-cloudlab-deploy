import random
import time
import target


def build_inputs(n):
    rnd = random.Random(1234)
    ops = ['__add__', '__sub__', '__mul__', 'BVV', 'And', 'Or', 'If', 'Extract']
    inputs = []
    for _ in range(n):
        op = rnd.choice(ops)
        nargs = rnd.randint(1, 4)
        args = tuple(rnd.randint(-(2**32), 2**32) for _ in range(nargs))
        keywords = {
            'length': rnd.choice([None, 8, 32, 64]),
            'variables': frozenset('v%d' % rnd.randint(0, 50) for _ in range(rnd.randint(0, 4))),
            'symbolic': rnd.choice([True, False]),
            'annotations': None,
        }
        inputs.append((op, args, keywords))
    return inputs


def main():
    inputs = build_inputs(2000)
    # warmup
    for op, args, keywords in inputs:
        target._calc_hash(op, args, keywords)

    N = 40
    start = time.perf_counter()
    acc = 0
    for _ in range(N):
        for op, args, keywords in inputs:
            acc ^= target._calc_hash(op, args, keywords)
    end = time.perf_counter()

    print("TIME_SECONDS=%r" % (end - start))


if __name__ == '__main__':
    main()
