"""Equivalence check: eui64 from BOTH target_before and target_after."""
import importlib.util


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


before = _load("target_before", "target_before.py")
after = _load("target_after", "target_after.py")


def run(mod, value, version):
    e = mod.EUI(value, version=version)
    before_value = e._value
    r = e.eui64()
    return {
        "result_value": r._value,
        "result_version": r.version,
        "result_str": str(r),
        "source_unmutated": e._value == before_value,
    }


def main():
    cases = [
        (0x000000000000, 48),          # empty / all zero
        (0x000000000001, 48),          # single bit
        (0x112233445566, 48),          # typical MAC
        (0xaabbccddeeff, 48),          # larger / all high
        (0xffffffffffff, 48),          # max EUI-48
        (0x0200000000000000, 64),      # already EUI-64 branch
        (0x1122334455667788, 64),      # typical EUI-64
    ]

    ok = True
    for value, version in cases:
        rb = run(before, value, version)
        ra = run(after, value, version)
        if rb != ra:
            ok = False
            print("MISMATCH value=0x%x version=%d" % (value, version))
            print("  before:", rb)
            print("  after :", ra)

    print("EQUIV=" + ("yes" if ok else "no"))


if __name__ == "__main__":
    main()
