import target_before
import target_after


def serialize(tokenlist):
    """Turn a token list into a comparable representation."""
    return [(type(t).__name__, dict(t.__dict__)) for t in tokenlist]


def run(mod, text):
    tok = mod.Tokenizer()
    result = tok.tokenize(list_safe(text))
    return result


def list_safe(text):
    return text


def main():
    inputs = [
        "",                                    # empty
        "x",                                   # single
        "Hello world",                         # plain text
        "a&amp;b",                             # single entity
        "text {{template|key=value|x}} more",  # typical with template
        "{{cite|title=T|year=2020}} and &nbsp; entity &lt; stuff " * 50,  # larger
    ]

    all_ok = True
    for text in inputs:
        rb = serialize(run(target_before, text))
        ra = serialize(run(target_after, text))
        if rb != ra:
            all_ok = False
            print("MISMATCH on input repr: {0!r}".format(text[:60]))
            print("  before:", rb[:5])
            print("  after :", ra[:5])

    print("EQUIV=yes" if all_ok else "EQUIV=no")


if __name__ == "__main__":
    main()
