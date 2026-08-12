import os
import sys

import target_before
import target_after

HERE = os.path.dirname(os.path.abspath(__file__))

unique_nonzip = [os.path.join(HERE, "dir_%03d" % i) for i in range(40)]
zips = [os.path.join(HERE, "zips", "z%d.zip" % i) for i in range(5)]
base = unique_nonzip + zips
PATH = base * 30


def cache_keys(fn):
    sys.path_importer_cache = {}
    pic = fn(list(PATH))
    # pic is sys.path_importer_cache; capture the set of keys added.
    return set(pic.keys())


before_keys = cache_keys(target_before._precache_zipimporters)
after_keys = cache_keys(target_after._precache_zipimporters)

print("EQUIV=%s" % ("yes" if before_keys == after_keys else "no"))
