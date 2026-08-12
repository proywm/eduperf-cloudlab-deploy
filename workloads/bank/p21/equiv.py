import random
from collections import defaultdict

import target_before
import target_after


def build_grid(seed=1234):
    rnd = random.Random(seed)
    plotwidth = 160
    plotheight = 120
    pixels = [[defaultdict(list) for _ in range(plotwidth)] for _ in range(plotheight)]
    npoints = int(plotwidth * plotheight * 0.05)
    attrs = list(range(1, 9))
    for _ in range(npoints):
        x = rnd.randrange(plotwidth)
        y = rnd.randrange(plotheight)
        attr = rnd.choice(attrs)
        for _ in range(rnd.randint(1, 3)):
            pixels[y][x][attr].append(object())
    return pixels, plotwidth, plotheight


def collect(mod, pixels, plotwidth, plotheight, hidden):
    p = mod.Plotter(pixels, hiddenAttrs=hidden, source=None)
    out = []
    for y in range(plotheight):
        for x in range(plotwidth):
            out.append(p.getPixelAttrMost(x, y))
    return out


def main():
    # use separate but identical grids; same seed -> same structure
    pb = build_grid()
    pa = build_grid()
    hidden = {3}
    rb = collect(target_before, *pb, hidden=set(hidden))
    ra = collect(target_after, *pa, hidden=set(hidden))
    print("EQUIV=%s" % ("yes" if rb == ra else "no"))


if __name__ == "__main__":
    main()
