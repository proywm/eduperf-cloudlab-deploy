import os
import tempfile
import shutil

import target_before
import target_after

TMP = tempfile.mkdtemp(prefix="trfh_equiv_")
BASE = os.path.join(TMP, "app.log")

# Mix of matching, non-matching, and decoy files
names = [
    "app.log.2020-01-05",
    "app.log.2019-12-31",
    "app.log.2021-06-15",
    "app.log.2018-03-03",
    "app.log.notadate",
    "other.log.2020-01-01",
    "app.log.2020-01-05.gz",
    "app.log.2017-11-22",
]
for nm in names:
    open(os.path.join(TMP, nm), "w").close()

ok = True
for bc in [0, 1, 2, 3, 4, 5, 100]:
    b = target_before.TimedRotatingFileHandler(BASE, backupCount=bc)
    a = target_after.TimedRotatingFileHandler(BASE, backupCount=bc)
    rb = b.getFilesToDelete()
    ra = a.getFilesToDelete()
    if rb != ra:
        ok = False
        break

shutil.rmtree(TMP, ignore_errors=True)
print("EQUIV=" + ("yes" if ok else "no"))
