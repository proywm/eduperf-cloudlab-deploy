def unique_list(seq, hashfunc=None):
    seen = {}
    if not hashfunc:
        return [x for x in seq
                if x not in seen
                and not seen.__setitem__(x, True)]
    else:
        return [x for x in seq
                if hashfunc(x) not in seen
                and not seen.__setitem__(hashfunc(x), True)]
