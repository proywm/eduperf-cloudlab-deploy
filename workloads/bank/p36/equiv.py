"""Equivalence check between before/after extractions of build_path_sets."""
import target_before as before_mod
import target_after as after_mod


class FakeGraph:
    def __init__(self, odd_nodes):
        self.odd_nodes = list(odd_nodes)


def run_case(odd_nodes):
    g1 = FakeGraph(odd_nodes)
    g2 = FakeGraph(odd_nodes)
    r1 = before_mod.build_path_sets(g1)
    r2 = after_mod.build_path_sets(g2)
    # Compare return values
    values_equal = (r1 == r2)
    # Compare in-place mutation of the input object
    mutation_equal = (g1.odd_nodes == g2.odd_nodes)
    return values_equal and mutation_equal


def main():
    cases = [
        [],            # empty
        [1, 2],        # single pair (M=2)
        [0, 1, 2, 3],  # typical small
        list(range(6)),   # larger
        list(range(8)),   # larger still
    ]
    ok = True
    for c in cases:
        if not run_case(c):
            ok = False
            break
    print('EQUIV=yes' if ok else 'EQUIV=no')


if __name__ == '__main__':
    main()
