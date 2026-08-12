import random
import time

import target
from target import Variable, debug_variables

random.seed(12345)

# Build a deterministic representative input.
# A realistic error line references a handful of names; some resolve, some don't.
names = []
locals_ = {}
globals_ = {}
for i in range(40):
    nm = f"var_{i}"
    names.append(nm)
    r = random.random()
    if r < 0.4:
        locals_[nm] = random.randint(0, 1000)
    elif r < 0.7:
        globals_[nm] = [random.random() for _ in range(3)]
    # else: name missing -> NameError/KeyError path

def make_variables():
    return [Variable(nm, col) for col, nm in enumerate(names)]

N = 20000
# warmup
debug_variables(make_variables(), dict(locals_), dict(globals_))

start = time.perf_counter()
for _ in range(N):
    vs = make_variables()
    debug_variables(vs, locals_, globals_)
end = time.perf_counter()

print(f"TIME_SECONDS={end - start}")
