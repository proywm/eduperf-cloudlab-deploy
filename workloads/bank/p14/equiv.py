import random

import target_before
import target_after


def build_vgs(mod, seed):
    rng = random.Random(seed)
    vgs = []
    for _ in range(200):
        n_pvs = rng.randint(40, 80)
        pvs = []
        for _ in range(n_pvs):
            if rng.random() < 0.6:
                ndisks = rng.randint(2, 8)
                disks = list(range(ndisks))
                pvs.append(mod.MDRaidArrayDevice(disks))
            else:
                pvs.append(mod.PlainPV())
        n_lvs = rng.randint(10, 30)
        lvs = [mod.LV(mod.Size(rng.randint(1, 1000))) for _ in range(n_lvs)]
        size = mod.Size(rng.randint(100000, 500000))
        reserved = mod.Size(rng.randint(0, 5000))
        vgs.append(mod.VG(pvs, lvs, size, reserved))
    return vgs


def main():
    before_vgs = build_vgs(target_before, 1234)
    after_vgs = build_vgs(target_after, 1234)

    ok = True
    for bvg, avg in zip(before_vgs, after_vgs):
        b = bvg.free_space
        a = avg.free_space
        if int(b) != int(a) or type(b) is not target_before.Size or type(a) is not target_after.Size:
            ok = False
            break

    print("EQUIV=%s" % ("yes" if ok else "no"))


if __name__ == "__main__":
    main()
