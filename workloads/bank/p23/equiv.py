import random

import target_before
import target_after


def make_sentences(n, seed):
    rng = random.Random(seed)
    sentences = []
    for _ in range(n):
        nlines = rng.randint(1, 10)
        lines = []
        for i in range(nlines):
            form = ''.join(rng.choice('abcdef') for _ in range(rng.randint(2, 6)))
            lines.append('%d\t%s' % (i + 1, form))
        sentences.append('\n'.join(lines))
    return sentences


def main():
    cases = [
        [],                         # empty
        ['1\ta'],                   # single
        make_sentences(5, 7),       # typical
        make_sentences(300, 99),    # larger
    ]

    for case in cases:
        b = target_before.conll(case)
        a = target_after.conll(case)
        if b != a:
            print('EQUIV=no')
            return

    print('EQUIV=yes')


if __name__ == '__main__':
    main()
