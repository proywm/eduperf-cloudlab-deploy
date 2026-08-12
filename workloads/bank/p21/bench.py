import random
import time
from collections import defaultdict

import target


def build_grid(seed=1234):
    rnd = random.Random(seed)
    # Realistic canvas: most pixels empty, a sparse scattering of plotted points.
    plotwidth = 320
    plotheight = 200
    pixels = [[defaultdict(list) for _ in range(plotwidth)] for _ in range(plotheight)]
    # Plot a sparse set of points/lines: ~3% of pixels get one or more attrs.
    npoints = int(plotwidth * plotheight * 0.03)
    attrs = list(range(1, 9))
    for _ in range(npoints):
        x = rnd.randrange(plotwidth)
        y = rnd.randrange(plotheight)
        attr = rnd.choice(attrs)
        # a few rows per (pixel, attr)
        for _ in range(rnd.randint(1, 3)):
            pixels[y][x][attr].append(object())
    return pixels, plotwidth, plotheight


def run():
    pixels, plotwidth, plotheight = build_grid()
    p = target.Plotter(pixels, hiddenAttrs=set(), source=None)
    total = 0
    # mimic draw(): iterate over all char cells, 8 pixels each
    for char_y in range(plotheight // 4):
        for char_x in range(plotwidth // 2):
            for (dx, dy) in ((0, 0), (0, 1), (0, 2), (1, 0), (1, 1), (1, 2), (0, 3), (1, 3)):
                a = p.getPixelAttrMost(char_x * 2 + dx, char_y * 4 + dy)
                total += a
    return total


def main():
    # warmup
    run()
    iters = 12
    t0 = time.perf_counter()
    last = 0
    for _ in range(iters):
        last = run()
    t1 = time.perf_counter()
    print("TIME_SECONDS=%f" % ((t1 - t0) / iters))
    print("CHECKSUM=%d" % last)


if __name__ == "__main__":
    main()
