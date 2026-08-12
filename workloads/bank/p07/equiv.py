import random

import target_before
import target_after


def build_node_attrs(seed, n_nodes, n_vars):
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
    return node_attrs, edges


def run(mod, node_attrs, edges):
    # fresh copies of the mutable sets per module
    na = {i: {'uses': set(v['uses']), 'def_var': v['def_var']}
          for i, v in node_attrs.items()}
    g = mod.SimpleGraph(na, list(edges))
    cfg = mod.ControlFlowGraph(g)
    return cfg.compute_liveness()


def main():
    ok = True
    for seed in range(25):
        node_attrs, edges = build_node_attrs(seed, 40, 10)
        bi, bo = run(target_before, node_attrs, edges)
        ai, ao = run(target_after, node_attrs, edges)
        if bi != ai or bo != ao:
            ok = False
            break
    print("EQUIV=%s" % ("yes" if ok else "no"))


if __name__ == '__main__':
    main()
