def build_closure(values):
    """Return a function whose __closure__ captures exactly `values` in order.

    Real closure cells require a nested function referencing enclosing
    locals, so we generate that structure dynamically.
    """
    if not values:
        def _inner():
            return 0
        return _inner  # __closure__ is None
    names = [f"a{i}" for i in range(len(values))]
    params = ", ".join(names)
    refs = ", ".join(names)
    src = (
        f"def _outer({params}):\n"
        f"    def _inner():\n"
        f"        return ({refs},)\n"
        f"    return _inner\n"
    )
    loc = {}
    exec(src, {}, loc)
    return loc["_outer"](*values)
