import random
import time

import target


def build_kwargs(n):
    rnd = random.Random(1234)
    return dict(
        id=1,
        external_id="ts",
        is_string=False,
        is_step=False,
        unit="m",
        timestamp=[1000 + i for i in range(n)],
        value=[rnd.random() for _ in range(n)],
        average=[rnd.random() for _ in range(n)],
        max=[rnd.random() for _ in range(n)],
        min=[rnd.random() for _ in range(n)],
        count=[rnd.randint(1, 10) for _ in range(n)],
        sum=[rnd.random() for _ in range(n)],
        interpolation=[rnd.random() for _ in range(n)],
        step_interpolation=[rnd.random() for _ in range(n)],
    )


def main():
    n = 2000
    iters = 400
    kwargs = build_kwargs(n)

    # warmup
    for _ in range(5):
        target.Datapoints(**kwargs).get_datapoint_objects()

    start = time.perf_counter()
    for _ in range(iters):
        dp = target.Datapoints(**kwargs)
        dp.get_datapoint_objects()
    elapsed = time.perf_counter() - start
    print("TIME_SECONDS={}".format(elapsed))


if __name__ == "__main__":
    main()
