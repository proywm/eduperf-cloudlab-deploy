import random

import target_before
import target_after

def make_input(seed):
    rng = random.Random(seed)
    nums = [str(rng.randint(0, 8)) for _ in range(300)]
    return ','.join(nums)

ok = True
for seed in range(50):
    s = make_input(seed)
    for iters in (0, 1, 7, 9, 80, 256, 1000):
        b = target_before.solve(s, iters)
        a = target_after.solve(s, iters)
        if b != a:
            ok = False
            print(f'MISMATCH seed={seed} iters={iters} before={b} after={a}')

print(f'EQUIV={"yes" if ok else "no"}')
