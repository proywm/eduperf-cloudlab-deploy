import time
import random
import target

Key = target.Key

# Deterministic representative input.
# In stig, Key instances are repeatedly re-wrapped via Key(key) (in mkkey,
# evaluate, KeyChain.__new__, keypress comparisons, etc.). Many of those Key
# values were produced by normalization (e.g. 'A' -> 'shift-a', '<alt-l>' ->
# 'alt-l') so the normalized string value is NOT the cache key (which is the
# original input string). Re-wrapping such Key instances forces a full re-parse
# in the 'before' version; 'after' short-circuits on isinstance.
random.seed(1234)

raw_keys = [
    'A', 'B', 'Z', '<alt-l>', '<ctrl-x>', 'meta g', 'esc', 'pos1', 'del',
    'ins', 'return', 'page up', 'page down', 'space', 'j', 'k', 'enter',
    'shift-E', '<F1>', 'home', 'a', 'b', 'c', 'd', 'e',
]

# Build the Key instances once (these go through full normalization).
key_objs = [Key(k) for k in raw_keys]

# Representative re-wrap workload: re-construct Key from existing Key instances.
N = 4000
work = [random.choice(key_objs) for _ in range(N)]


def run():
    total = 0
    for _ in range(20):
        for k in work:
            kk = Key(k)
            total += len(kk)
    return total


# warmup
run()

best = None
for _ in range(3):
    t0 = time.perf_counter()
    run()
    t1 = time.perf_counter()
    dt = t1 - t0
    if best is None or dt < best:
        best = dt

print("TIME_SECONDS=%r" % best)
