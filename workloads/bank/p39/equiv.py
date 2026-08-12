import random

import target_before
import target_after

random.seed(999)

chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:/.#-_~"
invalid = list('<>" {}|\\^`')

cases = []
# include edge cases
cases.append("")
cases.append("http://example.org/foo")
for ic in invalid:
    cases.append("http://example.org/" + ic + "bar")
    cases.append("http://example.org/bar" + ic)
# include some non-ASCII / high codepoint chars (ord > 256) to test the
# removed ord(c) <= 256 branch -- these are not in _invalid_uri_chars so
# both should treat them as valid
cases.append("http://example.org/café")
cases.append("http://example.org/€中文")
cases.append("  ")  # high unicode separators

for _ in range(5000):
    n = random.randint(0, 60)
    s = "".join(random.choice(chars + "".join(invalid) + "é€中") for _ in range(n))
    cases.append(s)

ok = True
for s in cases:
    if target_before._is_valid_uri(s) != target_after._is_valid_uri(s):
        ok = False
        break

print("EQUIV=" + ("yes" if ok else "no"))
