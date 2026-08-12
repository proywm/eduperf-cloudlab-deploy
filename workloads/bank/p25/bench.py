import random
import time

import target

def make_input(seed):
    rng = random.Random(seed)
    nums = [str(rng.randint(0, 8)) for _ in range(300)]
    return ','.join(nums)

INPUTS = [make_input(i) for i in range(20)]
ITERATIONS = 100000

def run():
    total = 0
    for s in INPUTS:
        total += target.solve(s, ITERATIONS)
    return total

# warm up
run()

start = time.perf_counter()
result = run()
elapsed = time.perf_counter() - start

print(f'RESULT={result}')
print(f'TIME_SECONDS={elapsed}')
