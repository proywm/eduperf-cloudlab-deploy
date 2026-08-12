"""Benchmark for build_path_sets. Imports a module named `target`."""
import time
import target


class FakeGraph:
    def __init__(self, odd_nodes):
        self.odd_nodes = odd_nodes


def main():
    # Deterministic representative input. M odd nodes -> C(M,2) pairs, then
    # combinations of those pairs taken M//2 at a time. M=10 -> ~1.2M outer
    # combos, enough to take well over a few milliseconds.
    odd_nodes = list(range(10))
    graph = FakeGraph(odd_nodes)

    N = 3
    # Warm up
    target.build_path_sets(graph)

    start = time.perf_counter()
    for _ in range(N):
        result = target.build_path_sets(graph)
    elapsed = time.perf_counter() - start

    # Touch result so it isn't optimized away
    _ = len(result)
    print('TIME_SECONDS={}'.format(elapsed))


if __name__ == '__main__':
    main()
