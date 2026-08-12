import random

import target_before
import target_after


def make_input(seed, n):
    rnd = random.Random(seed)
    return [rnd.randint(0, max(1, n // 4)) for _ in range(n)]


def cases():
    yield []                       # empty
    yield [42]                     # single
    yield make_input(1, 50)        # typical, with dups
    yield make_input(2, 5000)      # larger
    yield ["a", "b", "a", "c", "b"]  # strings


def main():
    ok = True
    hashfuncs = [None, (lambda x: x), (lambda x: hash(x) % 7)]
    for seq in cases():
        for hf in hashfuncs:
            try:
                b = target_before.unique_list(list(seq), hashfunc=hf)
            except TypeError:
                # hashfunc may not apply to string inputs uniformly; skip mismatched
                b = target_before.unique_list(list(seq))
                a = target_after.unique_list(list(seq))
                if b != a:
                    ok = False
                continue
            a = target_after.unique_list(list(seq), hashfunc=hf)
            if b != a:
                ok = False
    print("EQUIV=" + ("yes" if ok else "no"))


if __name__ == "__main__":
    main()
