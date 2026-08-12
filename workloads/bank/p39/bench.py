import random
import time

import target

random.seed(12345)

# Build a deterministic set of URI-like strings. Most are valid URIs;
# a small fraction contain an invalid char somewhere in the middle/end.
chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/.#-_~"
invalid = list('<>" {}|\\^`')

uris = []
for i in range(20000):
    n = random.randint(20, 120)
    s = "http://example.org/" + "".join(random.choice(chars) for _ in range(n))
    if i % 7 == 0:
        # insert an invalid char at a random position
        pos = random.randint(0, len(s))
        s = s[:pos] + random.choice(invalid) + s[pos:]
    uris.append(s)

f = target._is_valid_uri

start = time.perf_counter()
total = 0
for _ in range(40):
    for u in uris:
        if f(u):
            total += 1
elapsed = time.perf_counter() - start

print("CHECK=%d" % total)
print("TIME_SECONDS=%f" % elapsed)
