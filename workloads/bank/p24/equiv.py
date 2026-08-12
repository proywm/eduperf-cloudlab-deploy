import random

import target_before as before
import target_after as after


def make_inputs(n):
    rnd = random.Random(99)
    inputs = []
    for _ in range(n):
        code = rnd.choice([200, 404, 500])
        text = rnd.choice(["OK", "Not Found", "Server Error"])
        size = rnd.choice([0, 1, 64, 512, 4096])
        body = "".join(rnd.choice("abcXYZ012 é中") for _ in range(size))
        ctype = rnd.choice(["text/html", "text/plain", "application/json"])
        clen = rnd.choice([None, 12345])
        inputs.append((code, text, body, ctype, clen))
    return inputs


ok = True
for code, text, body, ctype, clen in make_inputs(200):
    sb = before.Status(code, text)
    sa = after.Status(code, text)
    hb = before.Headers()
    ha = after.Headers()

    rb = before._construct_response_bytes(
        status=sb, content_type=ctype, content_length=clen, headers=hb, body=body
    )
    ra = after._construct_response_bytes(
        status=sa, content_type=ctype, content_length=clen, headers=ha, body=body
    )

    if rb != ra:
        ok = False
        break
    # Compare header dict mutation too.
    if dict(hb) != dict(ha):
        ok = False
        break

print("EQUIV=yes" if ok else "EQUIV=no")
