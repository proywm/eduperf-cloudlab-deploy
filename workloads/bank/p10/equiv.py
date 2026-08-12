'''Equivalence check for Relation.selection before vs after.'''
import importlib

import target_before
import target_after


def make_rel(mod, header, rows):
    r = mod.relation()
    r.header = mod.Header(header)
    r.content = set(tuple(mod.rstring(str(v)) for v in row) for row in rows)
    return r


CASES = [
    # (header, rows, expr)
    (['age'], [], 'age > 10'),                                  # empty
    (['age'], [['42']], 'age > 10'),                            # single match
    (['age'], [['5']], 'age > 10'),                             # single no-match
    (['age', 'score', 'flag'],
     [['30', '70.5', '1'], ['10', '20.0', '0'], ['55', '90.1', '1'],
      ['40', '60.0', '0'], ['25', '10.0', '1']],
     '(age > 30 and score < 75.0) or (flag == 1 and age < 50)'),  # typical
    (['x'], [[str(i)] for i in range(200)], 'x % 3 == 0'),       # larger
]


def run():
    ok = True
    for header, rows, expr in CASES:
        rb = make_rel(target_before, header, rows)
        ra = make_rel(target_after, header, rows)
        before_in = set(rb.content)
        after_in = set(ra.content)

        res_b = rb.selection(expr)
        res_a = ra.selection(expr)

        # Compare returned content + header
        if set(res_b.content) != set(res_a.content):
            ok = False
        if tuple(res_b.header) != tuple(res_a.header):
            ok = False
        # Compare mutation of inputs (should be none)
        if set(rb.content) != before_in or set(ra.content) != after_in:
            ok = False

    print("EQUIV=" + ("yes" if ok else "no"))


if __name__ == '__main__':
    run()
