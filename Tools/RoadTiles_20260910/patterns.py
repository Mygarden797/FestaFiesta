"""Reference-based structural patterns for the wooden, cream and mint road tiles.

The public builder functions only use the shared Builder primitive API.  Distances
are metres.  Every visible unit is closed geometry resting on a continuous slab.
"""

import math


def wood(b):
    """Six broad crosswise deck planks, end fasteners, and restrained grain."""
    b.box("Foundation", (0, 0, 0.0275), (2, 2, 0.055), "WoodDark", bevel=0.021)
    plank_pitch = 2.0 / 6.0
    colors = ("Wood", "WoodLight", "Wood", "Wood", "WoodLight", "Wood")
    for row in range(6):
        y = -1.0 + (row + 0.5) * plank_pitch
        b.box("Plank_%02d" % row, (0, y, 0.0775),
              (1.988, plank_pitch - 0.006, 0.045), colors[row], bevel=0.011)
        for side in (-1, 1):
            b.cylinder("Bolt_%02d_%s" % (row, "L" if side < 0 else "R"),
                       (side * 0.921, y, 0.1008), 0.0105, 0.0024,
                       "Metal", n=8, detail=2)
        # Separate capped tubes preserve editability and are removed by LOD2.
        # Short strokes remain subtle at the intended game viewing distance.
        for stroke in range(3):
            start = -0.79 + 0.12 * ((row + stroke) % 3)
            length = 0.59 + 0.12 * ((2 * row + stroke) % 3)
            base_y = y + (-0.090 + stroke * 0.078)
            points = []
            for step in range(10):
                t = step / 9.0
                points.append((start + length * t,
                               base_y + 0.009 * math.sin(math.pi * t) *
                               math.sin(2.2 * math.pi * t + row),
                               0.1003))
            b.tube("Grain_%02d_%02d" % (row, stroke), points, 0.00075,
                   "WoodDark", n=4, detail=1)
        # A compact elongated knot echoes the image without becoming a texture.
        if row in (1, 3, 4):
            cx = 0.23 if row != 3 else -0.29
            cy = y + 0.035
            points = [(cx + 0.113 * math.cos(2 * math.pi * k / 18),
                       cy + 0.015 * math.sin(2 * math.pi * k / 18), 0.1003)
                      for k in range(17)]
            b.tube("Knot_%02d" % row, points, 0.00075,
                   "WoodDark", n=4, detail=1)


def _octagon(left, right, bottom, top, bl, br, tr, tl):
    """Counter-clockwise rectangle with independently clipped corners."""
    return [(left + bl, bottom), (right - br, bottom),
            (right, bottom + br), (right, top - tr),
            (right - tr, top), (left + tl, top),
            (left, top - tl), (left, bottom + bl)]


def cream(b):
    """Four-by-four cream pavers with five terracotta diamond inserts."""
    b.box("Foundation", (0, 0, 0.0275), (2, 2, 0.055), "Grout", bevel=0.021)
    diamonds = {(-0.5, -0.5), (0.5, -0.5), (0.0, 0.0),
                (-0.5, 0.5), (0.5, 0.5)}
    seam = 0.005
    inset = seam / 2.0
    for row in range(4):
        for col in range(4):
            x0, x1 = -1 + col * 0.5, -1 + (col + 1) * 0.5
            y0, y1 = -1 + row * 0.5, -1 + (row + 1) * 0.5
            corners = ((x0, y0), (x1, y0), (x1, y1), (x0, y1))
            cuts = [0.060 if p in diamonds else 0.009 for p in corners]
            # An additional diagonal seam separates each diamond from the tile.
            poly = _octagon(x0 + inset, x1 - inset, y0 + inset, y1 - inset,
                            *cuts)
            color = "CreamLight" if (row * 5 + col) % 6 == 0 else "Cream"
            b.prism("Paver_%02d_%02d" % (row, col), poly, 0.055, 0.100,
                    color, bevel=0.004)
    for index, (x, y) in enumerate(sorted(diamonds)):
        r = 0.060
        poly = [(x, y - r), (x + r, y), (x, y + r), (x - r, y)]
        b.prism("Terracotta_Diamond_%02d" % index, poly, 0.055, 0.100,
                "Sienna", bevel=0.0025)


def _area(poly):
    return 0.5 * sum(poly[i][0] * poly[(i + 1) % len(poly)][1] -
                     poly[(i + 1) % len(poly)][0] * poly[i][1]
                     for i in range(len(poly)))


def _clean(poly):
    result = []
    for p in poly:
        if not result or math.dist(p, result[-1]) > 1e-8:
            result.append(p)
    if len(result) > 1 and math.dist(result[0], result[-1]) < 1e-8:
        result.pop()
    return result


def _clip_half_plane(poly, normal, threshold):
    """Keep normal dot point >= threshold; input and output stay CCW."""
    if not poly:
        return []
    result = []
    previous = poly[-1]
    pd = previous[0] * normal[0] + previous[1] * normal[1] - threshold
    for current in poly:
        cd = current[0] * normal[0] + current[1] * normal[1] - threshold
        if (cd >= -1e-10) != (pd >= -1e-10):
            ratio = pd / (pd - cd)
            result.append((previous[0] + ratio * (current[0] - previous[0]),
                           previous[1] + ratio * (current[1] - previous[1])))
        if cd >= -1e-10:
            result.append(current)
        previous, pd = current, cd
    return _clean(result)


def _clip_central_rectangle(poly):
    for normal, threshold in (((1, 0), -0.8), ((-1, 0), -0.8),
                              ((0, 1), -1.0), ((0, -1), -1.0)):
        poly = _clip_half_plane(poly, normal, threshold)
    return poly


def _inset(poly, distance):
    """Uniform real grout distance, including at the boundary curb."""
    result = poly[:]
    for i, a in enumerate(poly):
        z = poly[(i + 1) % len(poly)]
        dx, dy = z[0] - a[0], z[1] - a[1]
        length = math.hypot(dx, dy)
        nx, ny = -dy / length, dx / length
        result = _clip_half_plane(result, (nx, ny), nx * a[0] + ny * a[1] + distance)
    return result


def _herringbone_cells(phase_x, phase_y):
    # An exact domino tessellation: horizontal anchors at (i+j)%4 == 0,
    # vertical anchors at (i+j)%4 == 2. Each integer square belongs to
    # exactly one brick. The entire tessellation is rotated by 45 degrees.
    unit = 0.245
    factor = math.sqrt(0.5)
    cells = []
    for j in range(-10, 11):
        for i in range(-10, 11):
            residue = (i + j) % 4
            if residue not in (0, 2):
                continue
            width, height = (2, 1) if residue == 0 else (1, 2)
            grid = [(i, j), (i + width, j),
                    (i + width, j + height), (i, j + height)]
            poly = [((x - y) * unit * factor + phase_x,
                     (x + y) * unit * factor + phase_y) for x, y in grid]
            raw = _clip_central_rectangle(poly)
            if len(raw) < 3 or _area(raw) < 1e-9:
                continue
            inner = _inset(raw, 0.003)
            if len(inner) < 3 or _area(inner) <= 0:
                return None
            cells.append((i, j, raw, inner))
    return cells


def _select_herringbone_phase():
    """Avoid thin corner fragments without deleting any structural pavers."""
    best = None
    # A deterministic phase search is small and uses no Blender operations.
    for px in (-0.09, -0.06, -0.03, 0.0, 0.03, 0.06, 0.09):
        for py in (-0.09, -0.06, -0.03, 0.0, 0.03, 0.06, 0.09):
            cells = _herringbone_cells(px, py)
            if cells is None:
                continue
            # The candidate with the largest smallest paver is the most robust
            # to beveling and triangle reduction. No clipped unit is discarded.
            score = min(_area(inner) for _, _, _, inner in cells)
            if best is None or score > best[0]:
                best = (score, cells)
    if best is None:
        raise ValueError("No valid herringbone tessellation phase")
    return best[1]


def mint(b):
    """True 45-degree herringbone paving bounded by segmented side curbs."""
    b.box("Foundation", (0, 0, 0.0275), (2, 2, 0.055), "MintDark", bevel=0.021)
    palette = ("Mint", "Mint", "MintLight", "Mint", "MintLight")
    for number, (i, j, raw, poly) in enumerate(_select_herringbone_phase()):
        # Limit bevel to each fragment's dimensions to preserve small end cuts.
        shortest = min(math.dist(poly[k], poly[(k + 1) % len(poly)])
                       for k in range(len(poly)))
        bevel = min(0.0035, shortest * 0.16)
        b.prism("Herringbone_%03d" % number, poly, 0.055, 0.100,
                palette[(i * 3 + j * 2) % len(palette)], bevel=bevel)
    for side in (-1, 1):
        for row in range(4):
            b.box("Curb_%s_%02d" % ("L" if side < 0 else "R", row),
                  (side * 0.9, -0.75 + 0.5 * row, 0.0775),
                  (0.194, 0.494, 0.045), "MintLight", bevel=0.007)
