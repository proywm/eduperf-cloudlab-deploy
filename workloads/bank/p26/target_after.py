def unique_list(seq, hashfunc=None):
    seen = set()
    seen_add = seen.add
    if not hashfunc:
        return [x for x in seq
                if x not in seen
                and not seen_add(x)]
    else:
        return [x for x in seq
                if hashfunc(x) not in seen
                and not seen_add(hashfunc(x))]
