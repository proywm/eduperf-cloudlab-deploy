import sys
import time

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Build deterministic input data
size = 200000
base_dict = {i: i for i in range(size)}
key = 12345
value = 9999

loops = 50

start = time.perf_counter()
for _ in range(loops):
    target.assoc(base_dict, key, value)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")