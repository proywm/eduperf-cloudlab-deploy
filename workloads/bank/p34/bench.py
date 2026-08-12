import os
import time
import tempfile
import shutil

import target

# Build a real directory of matching log files, deterministically.
TMP = tempfile.mkdtemp(prefix="trfh_bench_")
BASE = os.path.join(TMP, "app.log")

# Many matching backup files: app.log.YYYY-MM-DD
# Names inserted in reverse-sorted order so .sort() does real work.
N = 4000
names = []
for i in range(N):
    y = 2000 + (i % 100)
    m = (i % 12) + 1
    d = (i % 28) + 1
    names.append("app.log.%04d-%02d-%02d" % (9999 - i % 9000 if False else y, m, d))
# Generate genuinely varied, reverse-ish ordered suffixes
names = []
for i in range(N):
    # spread across a wide date range, inserted unsorted on disk
    total_days = (N - i)
    y = 1900 + (total_days // 365)
    rem = total_days % 365
    m = (rem // 28) + 1
    d = (rem % 28) + 1
    names.append("app.log.%04d-%02d-%02d" % (y, m, d))

for nm in names:
    open(os.path.join(TMP, nm), "w").close()

# backupCount larger than number of matches => result always shorter =>
# the after-version skips the sort entirely (the optimization path).
h = target.TimedRotatingFileHandler(BASE, backupCount=N + 1000)

# Warm up
h.getFilesToDelete()

start = time.perf_counter()
ITER = 400
for _ in range(ITER):
    h.getFilesToDelete()
elapsed = time.perf_counter() - start

shutil.rmtree(TMP, ignore_errors=True)
print("TIME_SECONDS=%f" % elapsed)
