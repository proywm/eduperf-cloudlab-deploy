import random
import time

import target


def make_inputs():
    rng = random.Random(1234)
    classes = ["gbold", "black", "grey", "highlight"]
    chars = "abcdefghijklmnopqrstuvwxyz ABCDEF 0123456789      (){}[]= "
    inputs = []
    for _ in range(2000):
        length = rng.randint(0, 120)
        text = "".join(rng.choice(chars) for _ in range(length))
        cls = rng.choice(classes)
        inputs.append((cls, text))
    return inputs


def main():
    inputs = make_inputs()
    iters = 60
    start = time.perf_counter()
    acc = 0
    for _ in range(iters):
        for cls, text in inputs:
            acc += len(target._colourify(cls, text))
    elapsed = time.perf_counter() - start
    # prevent dead-code elimination concerns
    if acc < 0:
        print(acc)
    print("TIME_SECONDS={}".format(elapsed))


if __name__ == "__main__":
    main()
