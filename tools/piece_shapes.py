"""Single source of truth for the chess piece artwork (100x100 box, y down).
Each piece = list of (kind, path): kind 'f' = filled body part, 'd' = dark detail drawn on top.
Path syntax: absolute M L C Q Z (SVG subset). gen_pieces.py flattens them for PS4 (C++), web (JS) and Android (Java)."""

def circle(cx, cy, r):
    k = 0.5523 * r
    return ("M %g %g C %g %g %g %g %g %g C %g %g %g %g %g %g C %g %g %g %g %g %g C %g %g %g %g %g %g Z" % (
        cx + r, cy, cx + r, cy + k, cx + k, cy + r, cx, cy + r, cx - k, cy + r, cx - r, cy + k, cx - r, cy,
        cx - r, cy - k, cx - k, cy - r, cx, cy - r, cx + k, cy - r, cx + r, cy - k, cx + r, cy))

def ellipse(cx, cy, rx, ry):
    kx, ky = 0.5523 * rx, 0.5523 * ry
    return ("M %g %g C %g %g %g %g %g %g C %g %g %g %g %g %g C %g %g %g %g %g %g C %g %g %g %g %g %g Z" % (
        cx + rx, cy, cx + rx, cy + ky, cx + kx, cy + ry, cx, cy + ry, cx - kx, cy + ry, cx - rx, cy + ky, cx - rx, cy,
        cx - rx, cy - ky, cx - kx, cy - ry, cx, cy - ry, cx + kx, cy - ry, cx + rx, cy - ky, cx + rx, cy))

BASE = [("f", "M 17 90 L 83 90 Q 87 90 87 86 L 87 83 Q 87 79 83 79 L 17 79 Q 13 79 13 83 L 13 86 Q 13 90 17 90 Z"),
        ("f", "M 22 79 L 78 79 L 75 73 L 25 73 Z")]

SHAPES = {
 'P': [("f", circle(50, 27, 11.5)),
       ("f", "M 35 42 Q 50 36 65 42 L 63 49 Q 50 54 37 49 Z"),
       ("f", "M 40 48 C 40 60 35 66 28 73 L 72 73 C 65 66 60 60 60 48 Z")] + BASE,
 'N': [("f", "M 24 79 C 23 67 27 60 35 54 C 40 50 41 47 37 46 C 31 47 25 52 21 51 C 16 50 15 45 19 40 "
             "C 24 33 31 25 37 18 L 39 9 L 46 15 C 50 13 56 13 61 17 C 73 26 79 46 77 63 C 76 69 76 73 78 79 Z"),
       ("d", circle(36, 29, 2.6)), ("d", circle(21.5, 44, 1.7)),
       ("d", "M 47 20 C 56 22 66 32 69 46 C 62 36 54 28 47 20 Z")] + BASE[:1],
 'B': [("f", circle(50, 13, 5.2)),
       ("f", "M 50 18 C 63 27 67 40 63 50 C 61 56 56 59 50 59 C 44 59 39 56 37 50 C 33 40 37 27 50 18 Z"),
       ("f", "M 37 58 Q 50 64 63 58 L 61 65 Q 50 70 39 65 Z"),
       ("f", "M 40 64 C 40 68 36 70 29 73 L 71 73 C 64 70 60 68 60 64 Z"),
       ("d", "M 47 29 L 50 26 L 58 41 L 55 44 Z")] + BASE,
 'R': [("f", "M 26 12 L 38 12 L 38 19 L 44 19 L 44 12 L 56 12 L 56 19 L 62 19 L 62 12 L 74 12 L 74 31 L 68 36 L 32 36 L 26 31 Z"),
       ("f", "M 32 35 L 68 35 L 64 42 L 36 42 Z"),
       ("f", "M 37 41 C 37 54 34 63 28 73 L 72 73 C 66 63 63 54 63 41 Z")] + BASE,
 'Q': [("f", circle(21, 25, 4.8)), ("f", circle(35.5, 15.5, 4.8)), ("f", circle(50, 11, 4.8)),
       ("f", circle(64.5, 15.5, 4.8)), ("f", circle(79, 25, 4.8)),
       ("f", "M 22 28 L 32 58 L 36 19 L 43 55 L 50 15 L 57 55 L 64 19 L 68 58 L 78 28 C 75 46 72 62 73 80 L 27 80 C 28 62 25 46 22 28 Z"),
       ("d", "M 31 62 Q 50 67 69 62 L 69 65 Q 50 70 31 65 Z")] + BASE[:1],
 'K': [("f", "M 46 3 L 54 3 L 54 10 L 61 10 L 61 17 L 54 17 L 54 24 L 46 24 L 46 17 L 39 17 L 39 10 L 46 10 Z"),
       ("f", "M 27 45 C 27 33 37 26 50 26 C 63 26 73 33 73 45 C 73 51 69 55 67 59 L 33 59 C 31 55 27 51 27 45 Z"),
       ("f", "M 33 58 C 33 66 30 72 26 80 L 74 80 C 70 72 67 66 67 58 Z"),
       ("d", "M 30 45 Q 50 51 70 45 L 70 48 Q 50 54 30 48 Z")] + BASE[:1],
}
