import time
import target

# Seed deterministic registered plugins (cache-hit fast path).
NAMES = ['plugin.{}'.format(i) for i in range(50)]
for n in NAMES:
    target.plugins_init_locks[n] = __import__('threading').Lock()
    target.plugins[n] = object()

ITERS = 200000

# warm up
for n in NAMES:
    target.get_plugin(n)

start = time.perf_counter()
for _ in range(ITERS):
    for n in NAMES:
        target.get_plugin(n)
elapsed = time.perf_counter() - start

print('TIME_SECONDS={:.6f}'.format(elapsed))
