import random

import target_before
import target_after


def make_env(rng):
    valid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"
    bad_extra = "-. /:@#"
    names = []
    for _ in range(40):
        n = rng.randint(3, 20)
        first = rng.choice("abcdefghijklmnopqrstuvwxyz_")
        rest = "".join(rng.choice(valid_chars) for _ in range(n))
        name = first + rest
        if rng.random() < 0.25:
            pos = rng.randint(0, len(name))
            name = name[:pos] + rng.choice(bad_extra) + name[pos:]
        names.append(name)
    return names


rng = random.Random(999)
envs = [make_env(rng) for _ in range(300)]
# Also include some edge cases.
envs.append(["", "1abc", "_ok", "CYLC_X", "has space", "ünïcode"])
envs.append([])

ok = True
for env in envs:
    if target_before.check_varnames(env) != target_after.check_varnames(env):
        ok = False
        break

print("EQUIV=" + ("yes" if ok else "no"))
