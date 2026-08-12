import sys
import time
import random
import string

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Find the isStringCjk callable
is_func = None
instance = None

# First try as a module-level function
func = getattr(target, 'isStringCjk', None)
if callable(func):
    is_func = func
else:
    # Search for a class that defines it
    for obj in vars(target).values():
        if isinstance(obj, type) and hasattr(obj, 'isStringCjk'):
            try:
                instance = obj()
                is_func = getattr(instance, 'isStringCjk')
                break
            except Exception:
                continue

if is_func is None:
    print("SKIP=No callable isStringCjk found")
    sys.exit(0)

# Build deterministic input data (large ASCII string, no CJK)
random.seed(0)
test_string = ''.join(random.choices(string.ascii_letters, k=2_000_000))

# Determine number of iterations to target ~0.5-1.5 seconds
iterations = 10

start = time.perf_counter()
for _ in range(iterations):
    is_func(test_string)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")