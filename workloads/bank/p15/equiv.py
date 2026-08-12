import random

import target_before
import target_after


def make_inputs(seed=99):
    rng = random.Random(seed)

    def addr(i):
        return "addr_%08x" % i

    n_addrs = 500
    addresses = [addr(i) for i in range(n_addrs)]

    txs = []
    for _ in range(50):
        outs = []
        for _ in range(6):
            if rng.random() < 0.7:
                a = addr(rng.randrange(n_addrs))
            else:
                a = "new_%08x" % rng.randrange(10 ** 9)
            outs.append(a)
        txs.append(outs)
    return addresses, txs


def run(mod):
    addresses, txs = make_inputs()
    results = []
    added = []
    for outs in txs:
        w = mod.Watcher(addresses)
        outputs = [mod.Output(a) for a in outs]
        results.append(w.inspect_outputs(outputs))
        added.append(list(w.added))
    return results, added


if __name__ == "__main__":
    rb = run(target_before)
    ra = run(target_after)
    print("EQUIV=" + ("yes" if rb == ra else "no"))
