from threading import Lock
import target_before
import target_after

NAMES = ['plugin.{}'.format(i) for i in range(50)]

sentinels = {n: object() for n in NAMES}

for n in NAMES:
    target_before.plugins_init_locks[n] = Lock()
    target_before.plugins[n] = sentinels[n]
    target_after.plugins_init_locks[n] = Lock()
    target_after.plugins[n] = sentinels[n]

ok = True
for n in NAMES:
    rb = target_before.get_plugin(n)
    ra = target_after.get_plugin(n)
    if rb is not ra:
        ok = False
        break

# Also check reload=True path raises identically (cache miss -> RuntimeError)
def raises(fn):
    try:
        fn()
        return None
    except RuntimeError as e:
        return str(e)

eb = raises(lambda: target_before.get_plugin(NAMES[0], reload=True))
ea = raises(lambda: target_after.get_plugin(NAMES[0], reload=True))
if eb != ea:
    ok = False

print('EQUIV=' + ('yes' if ok else 'no'))
