import target_before as B
import target_after as A

cases = [
    "",
    "E501",
    "E501,E711,W291",
    " E501 , , E711 ,  , W291 ,",
    ",".join((' E%d ' % (i % 1000)) for i in range(500)) + ",,  ,",
]
ok = True
for c in cases:
    rb = B._split_comma_separated(c)
    ra = A._split_comma_separated(c)
    if rb != ra:
        ok = False
        print("MISMATCH input=%r before=%r after=%r" % (c, rb, ra))
print("EQUIV=%s" % ("yes" if ok else "no"))
