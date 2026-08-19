import random
import time
import os

from target import consume_response


def make_chunks(rng, n_chunks, chunk_size):
    # Simulate the chunks the response iterator yields off the socket.
    # Construct DATA only (byte/str payloads), not cost.
    return [
        "".join(chr(rng.randint(33, 126)) for _ in range(chunk_size))
        for _ in range(n_chunks)
    ]


def main():
    rng = random.Random(1234)
    profile = os.environ.get("EDUPERF_PROFILE") == "1"
    # Many responses, each consumed from a number of socket-sized chunks.
    responses = [
        make_chunks(rng, 100 if profile else 600, 128 if profile else 256)
        for _ in range(6 if profile else 40)
    ]

    start = time.perf_counter()
    total = 0
    for _ in range(5 if profile else 40):
        for chunks in responses:
            data = consume_response(chunks)
            total += len(data)
    elapsed = time.perf_counter() - start

    print("CHECKSUM=%d" % total)
    print("TIME_SECONDS=%f" % elapsed)


if __name__ == "__main__":
    main()
