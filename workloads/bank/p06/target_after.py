import posixpath
import itertools
from collections import OrderedDict


def _parents(path):
    """
    Given a path with elements separated by
    posixpath.sep, generate all parents of that path.
    """
    return itertools.islice(_ancestry(path), 1, None)


def _ancestry(path):
    """
    Given a path with elements separated by
    posixpath.sep, generate all elements of that path
    """
    path = path.rstrip(posixpath.sep)
    while path and path != posixpath.sep:
        yield path
        path, tail = posixpath.split(path)


def _implied_dirs(names):
    parents = itertools.chain.from_iterable(map(_parents, names))
    # Cast names to a set for O(1) lookups
    existing = set(names)
    # Deduplicate entries in original order
    implied_dirs = OrderedDict.fromkeys(
        p + posixpath.sep for p in parents
        if p + posixpath.sep not in existing
    )
    return implied_dirs
