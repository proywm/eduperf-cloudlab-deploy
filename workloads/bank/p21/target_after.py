from collections import defaultdict


def anySelected(vs, rows):
    for r in rows:
        if vs.isSelected(r):
            return True


class Plotter:
    'extracted minimal Plotter holding pixel grid and hidden attrs'
    def __init__(self, pixels, hiddenAttrs, source=None):
        self.pixels = pixels
        self.hiddenAttrs = hiddenAttrs
        self.source = source

    def getPixelAttrMost(self, x, y):
        'most common attr at this pixel.'
        r = self.pixels[y][x]
        if not r:
            return 0
        c = [(len(rows), attr, rows) for attr, rows in r.items() if attr and attr not in self.hiddenAttrs]
        if not c:
            return 0
        _, attr, rows = max(c)
        # selected-row recoloring branch omitted (requires visidata color machinery);
        # source is None so isinstance(self.source, BaseSheet) is False in the original.
        return attr
