import random
import time

import target


def make_sentences(n, seed=1234):
    rng = random.Random(seed)
    sentences = []
    for _ in range(n):
        # Build a realistic-ish CoNLL-U sentence block as a single string.
        nlines = rng.randint(5, 30)
        lines = []
        for i in range(nlines):
            form = ''.join(rng.choice('abcdefghijklmnop') for _ in range(rng.randint(3, 10)))
            lines.append('%d\t%s\tlemma\tPOS\t_\t_\t0\troot\t_\t_' % (i + 1, form))
        sentences.append('\n'.join(lines))
    return sentences


def main():
    sentences = make_sentences(2000)

    iters = 400
    start = time.perf_counter()
    for _ in range(iters):
        out = target.conll(sentences)
    elapsed = time.perf_counter() - start

    # Prevent dead-code elimination concerns.
    if not out:
        raise SystemExit('empty output')

    print('TIME_SECONDS=%f' % elapsed)


if __name__ == '__main__':
    main()
