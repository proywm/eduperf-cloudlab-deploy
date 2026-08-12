import random
import time

import target


def make_inputs(seed=1234):
    rng = random.Random(seed)

    def addr(i):
        return "addr_%08x" % i

    # Watched address set (the wallet already watches these).
    n_addrs = 4000
    addresses = [addr(i) for i in range(n_addrs)]

    # Build many transactions, each with several outputs. Most outputs hit
    # already-watched addresses (the membership-test hot path).
    txs = []
    for _ in range(800):
        outs = []
        for _ in range(8):
            if rng.random() < 0.85:
                a = addr(rng.randrange(n_addrs))
            else:
                a = "new_%08x" % rng.randrange(10 ** 9)
            outs.append(target.Output(a))
        txs.append(outs)
    return addresses, txs


def run():
    addresses, txs = make_inputs()
    total = 0
    for outs in txs:
        w = target.Watcher(addresses)
        res = w.inspect_outputs(outs)
        total += len(res)
    return total


if __name__ == "__main__":
    run()  # warm up
    t0 = time.perf_counter()
    checksum = run()
    t1 = time.perf_counter()
    print("CHECKSUM=%d" % checksum)
    print("TIME_SECONDS=%f" % (t1 - t0))
