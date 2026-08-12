import random
import time

import target


def build_vgs(seed):
    rng = random.Random(seed)
    vgs = []
    for _ in range(200):
        n_pvs = rng.randint(40, 80)
        pvs = []
        for _ in range(n_pvs):
            if rng.random() < 0.6:
                ndisks = rng.randint(2, 8)
                disks = list(range(ndisks))
                pvs.append(target.MDRaidArrayDevice(disks))
            else:
                pvs.append(target.PlainPV())
        n_lvs = rng.randint(10, 30)
        lvs = [target.LV(target.Size(rng.randint(1, 1000)))
               for _ in range(n_lvs)]
        size = target.Size(rng.randint(100000, 500000))
        reserved = target.Size(rng.randint(0, 5000))
        vgs.append(target.VG(pvs, lvs, size, reserved))
    return vgs


def main():
    vgs = build_vgs(1234)
    total = 0
    start = time.perf_counter()
    for _ in range(400):
        for vg in vgs:
            total += int(vg.free_space)
    elapsed = time.perf_counter() - start
    print("CHECKSUM=%d" % total)
    print("TIME_SECONDS=%.6f" % elapsed)


if __name__ == "__main__":
    main()
