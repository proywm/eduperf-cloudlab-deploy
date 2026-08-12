import random

import target_before
import target_after


def make_blocks():
    rng = random.Random(1234)
    blocks = []
    for _ in range(40):
        blocks.append(''.join(chr(rng.randint(0, 255)) for _ in range(1024)))
    for _ in range(40):
        blocks.append(''.join(chr(rng.choice([0, 1, 2, 65, 200])) for _ in range(1024)))
    for _ in range(40):
        blocks.append(''.join(chr(rng.randint(0, 63)) for _ in range(1024)))
    # edge cases
    blocks.append('')
    blocks.append('A')
    return blocks


def main():
    blocks = make_blocks()
    ok = True
    for b in blocks:
        rb = target_before.shannon(b)
        ra = target_after.shannon(b)
        if rb != ra:
            ok = False
            break
    print("EQUIV=" + ("yes" if ok else "no"))


if __name__ == '__main__':
    main()
