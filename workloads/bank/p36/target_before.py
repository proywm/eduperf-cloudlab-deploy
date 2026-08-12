"""Standalone extraction of build_path_sets (BEFORE the perf change).

Only the changed function plus the minimal helpers it transitively calls are
kept. The original code imported a local `my_iter` module for `all_unique` and
`flatten_tuples`; those tiny helpers are reimplemented here with their obvious
semantics so the module runs on the standard library alone.

Note: the original repo targeted Python 2, where `len(odd_nodes) / 2` is
integer division. On Python 3 that yields a float, which `itertools.combinations`
rejects. We use `//` to faithfully reproduce the original Python-2 behavior.
This identical adjustment is applied to both before/after, so it does not
affect their equivalence or the measured perf difference.
"""
import itertools


def flatten_tuples(iterable):
    """ Flatten a tuple/iterable of pair-tuples into a flat list of nodes. """
    return [item for tup in iterable for item in tup]


def all_unique(iterable):
    """ Return True if every element in the iterable is unique. """
    seen = set()
    for item in iterable:
        if item in seen:
            return False
        seen.add(item)
    return True


def build_path_sets(graph):
    """ Builds all possible sets of odd node pairs. """
    odd_nodes = graph.odd_nodes
    combos = list(itertools.combinations(sorted(odd_nodes), 2))
    no_of_pairs = len(odd_nodes) // 2

    sets_2 = list(itertools.combinations(combos, no_of_pairs))
    sets_unique = [x for x in sets_2 if all_unique(flatten_tuples(x))]
    return sets_unique
