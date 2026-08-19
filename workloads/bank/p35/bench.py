import random
import time
import target
import os

random.seed(1234)
# Representative large input: many comma-separated tokens with whitespace.
tokens = []
for _ in range(2000):
    n = random.randint(1, 8)
    tokens.append(' ' * random.randint(0, 3) + 'E' + str(random.randint(100, 999)) + ' ' * random.randint(0, 3))
# include some empties to exercise the filter
tokens += ['', '  ', '']
s = ','.join(tokens)

f = target._split_comma_separated
ITERS = 200 if os.environ.get("EDUPERF_PROFILE") == "1" else 20000
start = time.perf_counter()
for _ in range(ITERS):
    f(s)
elapsed = time.perf_counter() - start
print("TIME_SECONDS=%f" % elapsed)
