import re


def check_varnames(env):
    """Check a list of env var names for legality.

    Return a list of bad names (empty implies success).
    """
    bad = []
    for varname in env:
        if not re.match(r'^[a-zA-Z_][\w]*$', varname):
            bad.append(varname)
    return bad
