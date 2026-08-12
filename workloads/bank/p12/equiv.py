import copy
from target_before import flatten_dict as fb
from target_after import flatten_dict as fa


def cases():
    return [
        {},                                   # empty
        {"a": 1},                             # single flat
        {"a": {"b": 2}},                      # single nested
        {"a": 1, "b": {"c": 2, "d": {"e": 3}}, "f": 4},  # typical
        {"l{}".format(i): {"m{}".format(j): {"n": i * j}
                           for j in range(5)} for i in range(10)},  # larger
    ]


def main():
    ok = True
    for c in cases():
        in_before = copy.deepcopy(c)
        in_after = copy.deepcopy(c)
        r_before = fb(in_before)
        r_after = fa(in_after)
        # compare return values
        if r_before != r_after:
            ok = False
            break
        # compare mutation of input
        if in_before != in_after:
            ok = False
            break
        # input should be unchanged vs original
        if in_before != c or in_after != c:
            ok = False
            break

    print("EQUIV=" + ("yes" if ok else "no"))


if __name__ == "__main__":
    main()
