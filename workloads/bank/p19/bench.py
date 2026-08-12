import random
import time
import target

random.seed(1234)
# Representative inputs: valid OUI integers in range [0, 0xffffff].
inputs = [random.randint(0, 0xffffff) for _ in range(2000)]

# Warmup
for v in inputs:
    target.oui_str(v)

best = None
for _ in range(5):
    t0 = time.perf_counter()
    for _ in range(50):
        for v in inputs:
            target.oui_str(v)
    t1 = time.perf_counter()
    dt = t1 - t0
    if best is None or dt < best:
        best = dt

print("TIME_SECONDS=%f" % best)
