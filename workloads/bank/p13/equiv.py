import random

import target_before as before
import target_after as after


def make_kwargs(n):
    rnd = random.Random(42)
    if n == 0:
        return dict(id=1, external_id="ts", is_string=False, is_step=False, unit="m", timestamp=[])
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
        count=[rnd.randint(1, 10) for _ in range(n)],
    )


def to_comparable(objs):
    return [
        (
            o.timestamp,
            o.value,
            o.average,
            o.max,
            o.count,
        )
        for o in objs
    ]


def main():
    all_equal = True
    for n in [0, 1, 5, 500]:
        kb = make_kwargs(n)
        ka = make_kwargs(n)
        b_objs = before.Datapoints(**kb).get_datapoint_objects()
        a_objs = after.Datapoints(**ka).get_datapoint_objects()
        if to_comparable(b_objs) != to_comparable(a_objs):
            all_equal = False
            break
        if len(b_objs) != n:
            all_equal = False
            break
    print("EQUIV={}".format("yes" if all_equal else "no"))


if __name__ == "__main__":
    main()
