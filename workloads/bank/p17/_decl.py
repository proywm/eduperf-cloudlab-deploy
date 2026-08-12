"""Shared mock declaration tree used by bench.py and equiv.py.

The real pygccxml declaration_t exposes:
  - decl.name                     (str)
  - decl.parent                   (declaration_t or None)
  - decl.cache.declaration_path   (list, falsy when not yet computed)

We reproduce only those attributes.
"""

class _Cache(object):
    __slots__ = ("declaration_path",)

    def __init__(self):
        self.declaration_path = None


class Decl(object):
    __slots__ = ("name", "parent", "cache")

    def __init__(self, name, parent=None):
        self.name = name
        self.parent = parent
        self.cache = _Cache()


def reset_cache(nodes):
    for n in nodes:
        n.cache.declaration_path = None


def build_chain(depth, seed=0):
    """Build a single deep parent chain of length `depth`.

    Returns (nodes_root_to_leaf). Names are deterministic.
    """
    import random
    rng = random.Random(seed)
    nodes = []
    parent = None
    for i in range(depth):
        # deterministic-ish but varied names
        name = "ns%d_%d" % (i, rng.randint(0, 1000000))
        node = Decl(name, parent)
        nodes.append(node)
        parent = node
    return nodes


def build_tree(depth, branching, seed=0):
    """Build a tree; return list of all nodes in BFS (root-first) order.

    Root-first order means when we iterate and call declaration_path on each
    node, every node's parent already has its cache populated -- which is the
    exact case the optimization is designed to accelerate.
    """
    import random
    rng = random.Random(seed)
    root = Decl("ns0_%d" % rng.randint(0, 1000000), None)
    all_nodes = [root]
    frontier = [root]
    cur_depth = 1
    while cur_depth < depth and frontier:
        next_frontier = []
        for p in frontier:
            for b in range(branching):
                name = "ns%d_%d_%d" % (cur_depth, b, rng.randint(0, 1000000))
                node = Decl(name, p)
                all_nodes.append(node)
                next_frontier.append(node)
        frontier = next_frontier
        cur_depth += 1
    return all_nodes
