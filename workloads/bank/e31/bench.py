import sys
import time

try:
    import target
    algorithms = {'foo': lambda: None}
    runner = target.Runner(algorithms)
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Warm‑up to avoid any one‑time overhead
_ = runner._get_algorithm('foo')

iterations = 10_000_000
start = time.perf_counter()
for _ in range(iterations):
    runner._get_algorithm('foo')
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")