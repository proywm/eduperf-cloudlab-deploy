import random
import time

import target

random.seed(1234)
# Representative input: a list of float datapoints aggregated with 'last'.
knownValues = [random.random() * 1000.0 for _ in range(1000)]

ITERS = 2000000
# warmup
for _ in range(1000):
    target.aggregate('last', knownValues)

start = time.perf_counter()
acc = 0.0
for _ in range(ITERS):
    acc += target.aggregate('last', knownValues)
end = time.perf_counter()

# keep acc alive
assert acc != 0.0
print("TIME_SECONDS=%f" % (end - start))
