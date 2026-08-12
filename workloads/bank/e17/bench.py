import time
import sys

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Ensure checksum is available and callable
checksum_func = getattr(target, "checksum", None)
if not callable(checksum_func):
    print("SKIP=checksum not found or not callable")
    sys.exit(0)

# Check if it's a plain function (not a bound method)
import inspect
if inspect.isfunction(checksum_func):
    pass
elif inspect.ismethod(checksum_func):
    # Bound method requires an instance; skip
    print("SKIP=checksum is a bound method requiring an instance")
    sys.exit(0)
else:
    print("SKIP=checksum is not a function")
    sys.exit(0)

# Build deterministic input data
SIZE = 8_000_000  # 8 MB of data
data_bytes = bytes([i % 256 for i in range(SIZE)])

# Test if checksum works with bytes; if not, try string
try:
    checksum_func(data_bytes)
    data = data_bytes
except Exception:
    try:
        data_str = data_bytes.decode("latin1")
        checksum_func(data_str)
        data = data_str
    except Exception as e:
        print(f"SKIP=checksum failed with bytes and string: {e}")
        sys.exit(0)

# Warm up and measure single call time
start = time.perf_counter()
checksum_func(data)
end = time.perf_counter()
single_time = end - start
if single_time == 0:
    single_time = 1e-9

# Determine number of iterations to target ~1 second
iterations = max(1, int(1.0 / single_time))
iterations = min(iterations, 100_000)

# Benchmark loop
start = time.perf_counter()
for _ in range(iterations):
    checksum_func(data)
end = time.perf_counter()
elapsed = end - start

print(f"TIME_SECONDS={elapsed}")