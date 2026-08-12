"""Benchmark parse_content."""
import random
import time

import target


def make_input(seed=0, n_lines=2000, line_len=80):
    rnd = random.Random(seed)
    words = ['alpha', 'beta', 'gamma', 'delta', '', 'epsilon', '   ', 'zeta', '¶']
    lines = []
    for _ in range(n_lines):
        ntok = rnd.randint(1, 12)
        toks = [rnd.choice(words) for _ in range(ntok)]
        # add leading/trailing whitespace to exercise strip()
        line = '  ' + ' '.join(toks) + '  ¶'
        lines.append(line)
    return '\n'.join(lines)


def main():
    data = make_input()
    iters = 2000

    # warmup
    for _ in range(50):
        target.parse_content(data)

    start = time.perf_counter()
    for _ in range(iters):
        target.parse_content(data)
    elapsed = time.perf_counter() - start

    print('TIME_SECONDS=%r' % (elapsed,))


if __name__ == '__main__':
    main()
