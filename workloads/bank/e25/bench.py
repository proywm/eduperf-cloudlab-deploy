import sys
import time
import random

try:
    import target
except Exception as e:
    print(f"SKIP=import error: {e}")
    sys.exit(0)

# Verify that gfo2hyper exists and is callable
gfo2hyper = getattr(target, "gfo2hyper", None)
if not callable(gfo2hyper):
    print("SKIP=gfo2hyper not found or not callable")
    sys.exit(0)

# Build deterministic input data
random.seed(0)
num_keys = 1000          # number of keys in search_space
list_len = 100           # length of each list in search_space

search_space = {}
for i in range(num_keys):
    key = f"k{i}"
    search_space[key] = list(range(list_len))

para = {}
for key in search_space:
    para[key] = random.randint(0, list_len - 1)

# Determine number of iterations to target ~0.5-1.5 seconds
# Rough estimate: each call processes num_keys items
# We'll start with 20000 iterations and adjust if needed
iterations = 20000

start = time.perf_counter()
for _ in range(iterations):
    gfo2hyper(search_space, para)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed:.6f}")