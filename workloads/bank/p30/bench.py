import random
import time

import target


def make_input(n_chars):
    """Deterministic representative wikitext of about n_chars characters."""
    random.seed(1234)
    # A pool of fragments resembling MediaWiki markup: plain text, templates,
    # template params, and HTML entities -- exercises all _parse branches.
    fragments = [
        "Lorem ipsum dolor sit amet ",
        "consectetur adipiscing elit ",
        "{{template|key=value|other}} ",
        "{{cite|title=Some Title|year=2020}} ",
        "text with &amp; and &lt; entities ",
        "more &nbsp; words and {{nested|a|b=c}} ",
        "plain running text without markup at all here ",
    ]
    parts = []
    total = 0
    while total < n_chars:
        frag = random.choice(fragments)
        parts.append(frag)
        total += len(frag)
    return "".join(parts)


def main():
    text = make_input(200000)
    N = 5

    # warmup
    t = target.Tokenizer()
    t.tokenize(text)

    start = time.perf_counter()
    for _ in range(N):
        tok = target.Tokenizer()
        tok.tokenize(text)
    elapsed = time.perf_counter() - start

    print("TIME_SECONDS={0}".format(elapsed))


if __name__ == "__main__":
    main()
