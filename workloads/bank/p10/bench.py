'''Benchmark for Relation.selection.

Builds a deterministic relation with many tuples and runs selection with a
non-trivial expression. The before-version re-parses the expression string on
every tuple via eval(); the after-version compiles it once. Speedup grows with
the number of tuples in the relation.
'''
import random
import time

import target


def build_relation():
    r = target.relation()
    r.header = target.Header(['age', 'score', 'name', 'flag'])
    rng = random.Random(1234)
    content = set()
    for n in range(4000):
        age = rng.randint(0, 99)
        score = round(rng.uniform(0, 100), 2)
        name = 'user%d' % n
        flag = rng.randint(0, 1)
        content.add((
            target.rstring(str(age)),
            target.rstring(str(score)),
            target.rstring(name),
            target.rstring(str(flag)),
        ))
    r.content = content
    return r


EXPR = '(age > 30 and score < 75.0) or (flag == 1 and age < 50)'


def main():
    r = build_relation()
    # Warm up
    r.selection(EXPR)

    iters = 30
    start = time.perf_counter()
    for _ in range(iters):
        r.selection(EXPR)
    elapsed = time.perf_counter() - start
    print("TIME_SECONDS=%r" % (elapsed,))


if __name__ == '__main__':
    main()
