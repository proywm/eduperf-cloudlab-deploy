import random
import time

import target


def build_graph(seed, n_nodes, n_vars):
    rng = random.Random(seed)
    var_names = ['v%d' % k for k in range(n_vars)]
    node_attrs = {}
    edges = []
    for i in range(n_nodes):
        uses = set(rng.sample(var_names, rng.randint(1, 3)))
        def_var = rng.choice(var_names) if rng.random() < 0.8 else None
        node_attrs[i] = {'uses': uses, 'def_var': def_var}
        if i > 0:
            edges.append((i - 1, i))
    return target.SimpleGraph(node_attrs, edges)


def main():
    # Several graphs, run liveness on each repeatedly.
    graphs = [build_graph(s, 50, 10) for s in range(15)]

    start = time.perf_counter()
    total = 0
    for _ in range(12):
        for g in graphs:
            cfg = target.ControlFlowGraph(g)
            live_in, live_out = cfg.compute_liveness()
            total += len(live_in) + len(live_out)
    elapsed = time.perf_counter() - start

    print("CHECK=%d" % total)
    print("TIME_SECONDS=%f" % elapsed)


if __name__ == '__main__':
    main()
