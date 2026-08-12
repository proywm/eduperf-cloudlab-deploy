import sys
import time
import inspect

# Try to import the target module
try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Locate a callable named _compile
compile_func = None
obj_class = None

# First, check if the module itself has _compile
if hasattr(target, "_compile") and callable(getattr(target, "_compile")):
    compile_func = getattr(target, "_compile")
else:
    # Search for a class that defines _compile
    for name, cls in inspect.getmembers(target, inspect.isclass):
        if hasattr(cls, "_compile") and callable(getattr(cls, "_compile")):
            # Try to instantiate the class without arguments
            try:
                instance = cls()
                compile_func = instance._compile
                obj_class = cls
                break
            except Exception:
                continue

if compile_func is None:
    print("SKIP=No _compile function found")
    sys.exit(0)

# Build deterministic input data
# A reasonably large pattern that is deterministic
pattern = "(a|b|c|d|e|f|g|h|i|j){1000}"
flags = 0

# Determine how to call the function
def try_call(*args):
    try:
        compile_func(*args)
        return True
    except Exception:
        return False

# Warm up: try calling with pattern only, then with flags if needed
use_flags = False
if try_call(pattern):
    use_flags = False
elif try_call(pattern, flags):
    use_flags = True
else:
    print("SKIP=compile_func call failed during warmup")
    sys.exit(0)

# Prepare arguments for timed loop
args = (pattern,) if not use_flags else (pattern, flags)

# Estimate time for a single call
sample_iters = 10
start = time.perf_counter()
for _ in range(sample_iters):
    compile_func(*args)
end = time.perf_counter()
single_time = (end - start) / sample_iters

# Choose number of iterations to target ~1 second, within 0.2-2 seconds
min_time = 0.2
max_time = 2.0
iters = max(1, int(1.0 / single_time))
# Adjust to stay within bounds
while single_time * iters < min_time:
    iters *= 2
while single_time * iters > max_time:
    iters //= 2
    if iters == 0:
        iters = 1
        break

# Timed loop
start = time.perf_counter()
for _ in range(iters):
    compile_func(*args)
end = time.perf_counter()
elapsed = end - start

print(f"TIME_SECONDS={elapsed}")