import time
import random

import target
from target import Stream
from _closure_util import build_closure


def make_input(n_streams, n_others):
    random.seed(1234)
    cells = [Stream() for _ in range(n_streams)] + [object() for _ in range(n_others)]
    random.shuffle(cells)
    return build_closure(cells)


fn = make_input(n_streams=400, n_others=400)

# Warmup / sanity
_ = target.input_streams(fn)

ITERS = 30000
start = time.perf_counter()
total = 0
for _ in range(ITERS):
    total += len(target.input_streams(fn))
elapsed = time.perf_counter() - start

print("CHECKSUM=%d" % total)
print("TIME_SECONDS=%f" % elapsed)
