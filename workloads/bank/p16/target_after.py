import re

RE_VARNAME = re.compile(r'^[a-zA-Z_][\w]*$')


def check_varnames(env):
    """Check a list of env var names for legality.

    Return a list of bad names (empty implies success).
    """
    bad = []
    for varname in env:
        if not RE_VARNAME.match(varname):
            bad.append(varname)
    return bad
