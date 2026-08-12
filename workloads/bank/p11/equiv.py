import target_before
import target_after


def main():
    cases = [
        ("black", ""),
        ("grey", "x"),
        ("grey", " "),
        ("gbold", "hello world"),
        ("highlight", "  leading and  multiple   spaces  "),
        ("black", "no_spaces_here"),
        ("c", "a b c d e f g h i j " * 20),
        ("class", "\ttab and space mix \n"),
    ]
    ok = True
    for cls, text in cases:
        rb = target_before._colourify(cls, text)
        ra = target_after._colourify(cls, text)
        if rb != ra:
            ok = False
            print("MISMATCH for {!r}: {!r} != {!r}".format((cls, text), rb, ra))
    print("EQUIV=" + ("yes" if ok else "no"))


if __name__ == "__main__":
    main()
