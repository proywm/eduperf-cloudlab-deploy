import random
import time

import target


def make_inputs(n):
    rnd = random.Random(1234)
    inputs = []
    for _ in range(n):
        code = rnd.choice([200, 404, 500])
        text = rnd.choice(["OK", "Not Found", "Server Error"])
        # Body sizes spanning small to large; include multibyte to exercise utf-8.
        size = rnd.choice([64, 512, 4096, 32768])
        body = "".join(rnd.choice("abcdefABCDEF0123 é中") for _ in range(size))
        inputs.append((target.Status(code, text), body))
    return inputs


def run():
    inputs = make_inputs(40)
    total = 0
    for _ in range(120):
        for status, body in inputs:
            headers = target.Headers()
            out = target._construct_response_bytes(
                http_version="HTTP/1.1",
                status=status,
                content_type="text/html",
                headers=headers,
                body=body,
            )
            total += len(out)
    return total


# warmup
run()

times = []
for _ in range(3):
    t0 = time.perf_counter()
    checksum = run()
    times.append(time.perf_counter() - t0)

times.sort()
print(f"TIME_SECONDS={times[1]}")
print(f"CHECKSUM={checksum}")
