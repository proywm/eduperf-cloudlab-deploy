import os
import sys
import time

import target

HERE = os.path.dirname(os.path.abspath(__file__))
PROFILE = os.environ.get("EDUPERF_PROFILE") == "1"

# Deterministic input: a path list with many duplicate non-zip directory
# entries (which always fail zipimporter and are never cached), plus a few
# real zip files. Failed entries are NOT added to the cache, so the original
# code re-attempts zipimport.zipimporter on every duplicate occurrence.
unique_nonzip = [os.path.join(HERE, "dir_%03d" % i) for i in range(40)]
zips = [os.path.join(HERE, "zips", "z%d.zip" % i) for i in range(5)]

base = unique_nonzip + zips
# Repeat to create many duplicate occurrences of the failing entries.
PATH = base * (3 if PROFILE else 30)  # bounded profile; full runtime keeps 1350 entries


def run():
    total = 0.0
    for _ in range(5 if PROFILE else 40):
        # Fresh empty cache each iteration so failed (non-zip) entries are
        # never cached -> original code re-attempts them on every duplicate.
        sys.path_importer_cache = {}
        pic = target._precache_zipimporters(list(PATH))
        total += len(pic)
    return total


# warmup
run()

times = []
for _ in range(3):
    t0 = time.perf_counter()
    r = run()
    times.append(time.perf_counter() - t0)

times.sort()
print("RESULT_CHECK=%d" % r)
print("TIME_SECONDS=%f" % times[1])
