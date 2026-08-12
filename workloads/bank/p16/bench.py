import random
import time

import target


def make_env(rng):
    # Mix of valid and invalid env var names.
    valid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"
    bad_extra = "-. /:@#"
    names = []
    for _ in range(40):
        n = rng.randint(3, 20)
        first = rng.choice("abcdefghijklmnopqrstuvwxyz_")
        rest = "".join(rng.choice(valid_chars) for _ in range(n))
        name = first + rest
        if rng.random() < 0.25:
            # Inject an illegal character to exercise the no-match path too.
            pos = rng.randint(0, len(name))
            name = name[:pos] + rng.choice(bad_extra) + name[pos:]
        names.append(name)
    return names


rng = random.Random(12345)
envs = [make_env(rng) for _ in range(300)]

# Warmup
for env in envs[:10]:
    target.check_varnames(env)

start = time.perf_counter()
total = 0
for _ in range(400):
    for env in envs:
        total += len(target.check_varnames(env))
elapsed = time.perf_counter() - start

print("CHECKSUM=%d" % total)
print("TIME_SECONDS=%f" % elapsed)
