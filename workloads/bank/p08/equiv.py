import random

from target_before import consume_response as before
from target_after import consume_response as after


def make_chunks(rng, n_chunks, chunk_size):
    return [
        "".join(chr(rng.randint(33, 126)) for _ in range(chunk_size))
        for _ in range(n_chunks)
    ]


def main():
    ok = True
    for seed in range(20):
        rng = random.Random(seed)
        chunks = make_chunks(rng, rng.randint(0, 50), rng.randint(0, 64))
        rb = before(list(chunks))
        ra = after(list(chunks))
        if rb != ra:
            ok = False
            break
    print("EQUIV=%s" % ("yes" if ok else "no"))


if __name__ == "__main__":
    main()
