from target_before import _calc_hash as before
from target_after import _calc_hash as after


def cases():
    out = []
    # empty args
    out.append(('BVV', (), {'variables': frozenset(), 'symbolic': False}))
    # single arg
    out.append(('__add__', (5,), {'variables': frozenset(['x']), 'symbolic': True, 'length': 32}))
    # typical
    out.append(('And', (1, 2, 3), {'variables': frozenset(['a', 'b']), 'symbolic': True,
                                    'length': 64, 'annotations': None}))
    # float arg + no length/annotations keys
    out.append(('__mul__', (3.5, -7), {'variables': frozenset(['z']), 'symbolic': False}))
    # larger
    big_args = tuple(range(50))
    out.append(('Concat', big_args, {'variables': frozenset('v%d' % i for i in range(30)),
                                      'symbolic': True, 'length': 128, 'annotations': None}))
    return out


def main():
    ok = True
    for op, args, keywords in cases():
        b = before(op, args, dict(keywords))
        a = after(op, args, dict(keywords))
        if b != a:
            ok = False
            print("MISMATCH op=%r b=%r a=%r" % (op, b, a))
    print("EQUIV=" + ("yes" if ok else "no"))


if __name__ == '__main__':
    main()
