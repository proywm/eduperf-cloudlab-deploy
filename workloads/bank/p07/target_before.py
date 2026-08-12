import copy


class SimpleGraph(object):
    """Minimal stand-in for the networkx DiGraph used by ControlFlowGraph.

    This is pure input DATA: it stores node attributes and adjacency.
    It does NOT simulate any cost (no sleeps, no fake latency).
    """

    def __init__(self, node_attrs, edges):
        # node_attrs: dict node_id -> {'uses': set, 'def_var': str|None}
        self.node = node_attrs
        self._succ = {i: [] for i in node_attrs}
        for s, d in edges:
            self._succ[s].append(d)

    def __iter__(self):
        return iter(self.node)

    def successors(self, i):
        return self._succ[i]


class ControlFlowGraph(object):
    def __init__(self, graph):
        self.graph = graph

    def compute_liveness(self):
        """Run liveness analysis over the control flow graph.

        :returns: A tuple containing live_in, live_out dictionaries.  The keys
        are variable names (strings) and the values are string sets.
        """

        # All variables that are accessed are live-in at a node
        live_in = {i: copy.copy(self.graph.node[i]['uses'])
                   for i in self.graph}
        live_out = {i: set() for i in self.graph}

        while True:
            live_in_prev = copy.deepcopy(live_in)
            live_out_prev = copy.deepcopy(live_out)

            for i in self.graph:
                # live out variables that are not defined are live-in
                def_var = self.graph.node[i]['def_var']
                def_set = set()
                if def_var is not None:
                    def_set.add(def_var)
                live_in[i].update(live_out_prev[i] - def_set)

                # variables that are live-in at a successor are live-out
                for successor in self.graph.successors(i):
                    live_out[i].update(live_in_prev[successor])

            if live_in == live_in_prev and live_out == live_out_prev:
                return live_in, live_out
