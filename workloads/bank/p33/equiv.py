import importlib.util

from _closure_util import build_closure


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


before = load("target_before.py", "before_mod")
after = load("target_after.py", "after_mod")


def streams(mod, n):
    return [mod.Stream() for _ in range(n)]


ok = True


def check(cells_b, cells_a, expected_len):
    global ok
    fb = build_closure(cells_b)
    fa = build_closure(cells_a)
    rb = before.input_streams(fb)
    ra = after.input_streams(fa)
    if len(rb) != expected_len or len(ra) != expected_len:
        ok = False
    # Order preservation: result must equal the Stream cells in actual closure order.
    exp_b = ([c.cell_contents for c in (fb.__closure__ or [])
              if isinstance(c.cell_contents, before.Stream)])
    exp_a = ([c.cell_contents for c in (fa.__closure__ or [])
              if isinstance(c.cell_contents, after.Stream)])
    if rb != exp_b or ra != exp_a:
        ok = False


# Case: empty (no closure -> __closure__ is None)
check([], [], 0)

# Case: single stream
check(streams(before, 1), streams(after, 1), 1)

# Case: single non-stream
check([object()], [object()], 0)

# Case: typical mix
sb = streams(before, 3)
sa = streams(after, 3)
check([sb[0], object(), sb[1], 42, sb[2]],
      [sa[0], object(), sa[1], 42, sa[2]], 3)

# Case: larger interleaved
sb = streams(before, 50)
sa = streams(after, 50)
cb, ca = [], []
for i in range(50):
    cb += [sb[i], object()]
    ca += [sa[i], object()]
check(cb, ca, 50)

print("EQUIV=%s" % ("yes" if ok else "no"))
