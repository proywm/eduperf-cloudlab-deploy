import random

import target_before
import target_after


def make_names(n_files, depth, seed):
    rng = random.Random(seed)
    components = ['dir%d' % i for i in range(10)]
    names = []
    for _ in range(n_files):
        d = rng.randint(1, depth)
        parts = [rng.choice(components) for _ in range(d)]
        name = '/'.join(parts) + '/file%d.txt' % rng.randint(0, 100)
        names.append(name)
    return names


def main():
    cases = {
        'empty': [],
        'single': ['a/b/c.txt'],
        'typical': ['a.txt', 'b/c.txt', 'b/d/e.txt'],
        'larger': make_names(300, 5, seed=7),
    }

    all_ok = True
    for label, names in cases.items():
        before_in = list(names)
        after_in = list(names)
        rb = target_before._implied_dirs(before_in)
        ra = target_after._implied_dirs(after_in)
        # compare return: OrderedDict keys + order
        same_return = list(rb.keys()) == list(ra.keys())
        # compare mutation of inputs
        same_mut = before_in == after_in == list(names)
        if not (same_return and same_mut):
            all_ok = False
            print("MISMATCH in %s: return=%s mut=%s" % (label, same_return, same_mut))

    print("EQUIV=%s" % ("yes" if all_ok else "no"))


if __name__ == "__main__":
    main()
