#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Genera sprite RGB565 per Maze Mouse (PNG + sprites.h)."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "games" / "maze_mouse" / "sprites"
HDR = ROOT / "games" / "maze_mouse" / "sprites.h"

# Palette RGBA
_ = (0, 0, 0, 0)
N = (16, 12, 10, 255)       # contorno
W = (255, 255, 255, 255)
K = (20, 16, 14, 255)       # pupilla / naso scuro

I = (255, 170, 180, 255)    # naso / interno orecchio

# Muri
BR = (168, 92, 58, 255)
BR_L = (198, 120, 78, 255)
BR_D = (120, 60, 36, 255)
MOR = (72, 40, 28, 255)
PIPE = (150, 158, 170, 255)
SHINE = (220, 226, 234, 255)
WOOD = (110, 70, 42, 255)
WOOD_L = (140, 92, 56, 255)

G_PAL = {
    "arancio": {"B": (255, 148, 40, 255), "D": (210, 100, 20, 255), "E": (255, 190, 120, 255)},
    "grigio": {"B": (176, 180, 190, 255), "D": (120, 124, 134, 255), "E": (220, 224, 230, 255)},
    "marrone": {"B": (110, 78, 58, 255), "D": (70, 48, 34, 255), "E": (150, 112, 84, 255)},
    "crema": {"B": (230, 200, 150, 255), "D": (180, 140, 95, 255), "E": (250, 230, 190, 255)},
}
SLEEP_B = (104, 124, 176, 255)
SLEEP_D = (70, 88, 136, 255)
SLEEP_E = (150, 168, 210, 255)
EYE_B = (40, 80, 220, 255)

# Frutta 16x16 ricalcata dal foglio pixel-art (mela, anguria, banana, arancia,
# ciliegie, ananas, uva, limone, fragola).

# Sheet gatti: righe 5-8, colonne 7-9 (32x32), poi ridotti a 16x16.
SHEET = OUT_DIR / "cats_sheet.png"
MOUSE_SHEET = OUT_DIR / "mouse_sheet.png"
# Sheet topo 192x432: 4 colonne da 48 px.
# Righe: camminata destra/giu/su, scatto, attacco. La sinistra e' lo specchio.
MOUSE_CELL = 48
MOUSE_BANDS = [
    ("destra", 0, 40, 32),
    ("giu", 40, 72, 32),
    ("su", 72, 104, 32),
    ("corsa_destra", 104, 132, 48),
    ("corsa_giu", 132, 168, 48),
    ("corsa_su", 168, 210, 48),
    ("attacco_destra", 210, 248, 48),
    ("attacco_giu", 248, 288, 48),
    ("attacco_su", 288, 340, 48),
]
SHEET_DIRS = [("giu", 5), ("sinistra", 6), ("destra", 7), ("su", 8)]
SHEET_COLS = (7, 8, 9)  # 3 frame camminata
CAT_TARGETS = {
    "arancio": (255, 148, 40),
    "grigio": (176, 180, 190),
    "marrone": (174, 106, 55),  # tinta originale dello sheet
    "crema": (230, 200, 150),
}


def save_png(path: Path, w: int, h: int, pixels: list) -> None:
    raw = b""
    for y in range(h):
        raw += b"\x00"
        for x in range(w):
            r, g, b, a = pixels[y * w + x]
            raw += bytes((r, g, b, a))

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 9))
    data += chunk(b"IEND", b"")
    path.write_bytes(data)


def rgb565(px) -> int:
    r, g, b, a = px
    if a < 128:
        return 0xF81F  # magenta key
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def flip_x(grid: list, w: int = 16, h: int = 16) -> list:
    out = []
    for y in range(h):
        row = grid[y * w : (y + 1) * w]
        out.extend(reversed(row))
    return out


def paint(rows: list[str], table: dict) -> list:
    h = len(rows)
    w = len(rows[0])
    out = []
    for row in rows:
        assert len(row) == w, (w, len(row), row)
        for ch in row:
            out.append(table[ch])
    return out


def cat_face(pal: dict, eyes: str) -> list:
    """Faccia frontale da gatto (stile emoji): orecchie a V alte, occhi chiari, naso.
    eyes: 'open' | 'left' | 'right' | 'closed'
    """
    B, Dd, Ee = pal["B"], pal["D"], pal["E"]
    if eyes == "closed":
        e1, e2 = ".NBBNNNNNNBNBN..", ".NBNBNNNNBNBN..."
    elif eyes == "left":
        e1, e2 = ".NKWWBNNBKWWBN..", ".NBBBBBNBBBBBN.."
    elif eyes == "right":
        e1, e2 = ".NWWKBNNBWWKBN..", ".NBBBBBNBBBBBN.."
    else:
        e1, e2 = ".NWWKBNNBWWKBN..", ".NBBBBBNBBBBBN.."

    # Orecchie che partono dalla riga 0, testa tonda, coda assente (vista frontale).
    rows = [
        "N..............N",
        "NEN..........NEN",
        ".NEN........NEN.",
        ".NEEBBBBBBBBEEBN",
        ".NBBBBBBBBBBBN..",
        e1,
        e2,
        ".NBBBBNINBBBBBN.",
        ".NBBBBBBBBBBBN..",
        "..NBBDBBDBBBN...",
        "..NB.NB.NB.N....",
        "...N..N..N......",
        "................",
        "................",
        "................",
        "................",
    ]
    return paint(
        rows,
        {".": _, "N": N, "B": B, "D": Dd, "E": Ee, "W": W, "K": K, "I": I},
    )


def cat_right(pal: dict) -> list:
    # Usa faccia con sguardo a destra (piu' leggibile del profilo confuso).
    return cat_face(pal, "right")


def cat_down(pal: dict) -> list:
    return cat_face(pal, "open")


def cat_up(pal: dict) -> list:
    """Di spalle: orecchie + coda a pennacchio."""
    B, Dd, Ee = pal["B"], pal["D"], pal["E"]
    rows = [
        "................",
        ".......NN.......",
        "......NBNN......",
        "......NBBBN.....",
        "..N...NBBBN..N..",
        ".NEN.NBBBBBNEN..",
        ".NEEBBBBBBBEEBN.",
        ".NBBBBBBBBBBBN..",
        ".NBBBBBBBBBBBN..",
        "..NBBBBBBBBBN...",
        "..NB.NB.NB.N....",
        "...N..N..N......",
        "................",
        "................",
        "................",
        "................",
    ]
    return paint(rows, {".": _, "N": N, "B": B, "D": Dd, "E": Ee, "W": W, "K": K})


def cat_sleep() -> list:
    return cat_face(
        {"B": SLEEP_B, "D": SLEEP_D, "E": SLEEP_E},
        "closed",
    )


def cat_eyes() -> list:
    rows = [
        "................",
        "................",
        "................",
        "................",
        "...WWW....WWW...",
        "..WKKKW..WKKKW..",
        "..WKKKW..WKKKW..",
        "...WWW....WWW...",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]
    return paint(rows, {".": _, "W": W, "K": EYE_B})


def cheese() -> list:
    # Terza casella in alto a sinistra dello sheet formaggi: spicchio a destra.
    # N contorno, C buco chiaro, Y pasta, O ombra.
    rows = [
        "................",
        "................",
        ".....NNNNN......",
        "....NCCCYCNN....",
        "...NCOOCCCCCNN..",
        "..NCCYYCCCCCYYN.",
        "..NCCCCCYCOYOYN.",
        ".NCOOCCYOYYYYYN.",
        ".NOYYOOYYYYYYYN.",
        ".NYYYOOYYOOOYON.",
        ".NYYYYYYYOONNN..",
        ".NOOYYOYNNN.....",
        ".NOOYNNN........",
        "..NNN...........",
        "................",
        "................",
    ]
    return paint(
        rows,
        {
            ".": _,
            "N": (6, 6, 8, 255),
            "C": (254, 243, 192, 255),
            "Y": (255, 213, 65, 255),
            "O": (249, 163, 27, 255),
        },
    )


def bottle() -> list:
    # Bottiglia di latte 16x16: tappo blu, collo stretto, corpo crema.
    rows = [
        "................",
        "...NNNNNNNNNN...",
        "...NBBBBBBBBN...",
        "...NBBBBBBBBN...",
        "....NNNNNNNN....",
        ".....NWWWWN.....",
        "....NWWWWWN.....",
        "...NWWWWWWWN....",
        "..NLWWWWWWWWN...",
        "..NWWWWWWWWWN...",
        "..NWWWWWWWWWN...",
        "..NWWWWWWWWWN...",
        "..NWWWWWWWWWN...",
        "..NWWWWWWWWSN...",
        "...NNNNNNNNN....",
        "................",
    ]
    return paint(
        rows,
        {
            ".": _,
            "N": (6, 6, 8, 255),
            "B": (28, 159, 215, 255),
            "W": (251, 247, 236, 255),
            "L": (255, 255, 255, 255),
            "S": (214, 204, 190, 255),
        },
    )


def wall() -> list:
    rows = [
        "LHHHLHHHLHHHLHHH",
        "HDDDHDHDDHDDDHDH",
        "HDDDHDHDDHDDDHDH",
        "NNNNNNNNNNNNNNNN",
        "HLHHHLHHHLHHHLHH",
        "HDHDDDHDHDDDHDHD",
        "HDHDDDHDHDDDHDHD",
        "NNNNNNNNNNNNNNNN",
        "LHHHLHHHLHHHLHHH",
        "HDDDHDHDDHDDDHDH",
        "HDDDHDHDDHDDDHDH",
        "NNNNNNNNNNNNNNNN",
        "HLHHHLHHHLHHHLHH",
        "HDHDDDHDHDDDHDHD",
        "HDHDDDHDHDDDHDHD",
        "SSSSSSSSSSSSSSSS",
    ]
    return paint(rows, {"L": BR_L, "H": BR, "D": BR_D, "N": MOR, "S": MOR})


def wall_pipe() -> list:
    rows = [
        "LHHHLHHHLHHHLHHH",
        "HDDDHDHDDHDDDHDH",
        "HDDDHDHDDHDDDHDH",
        "NNNNNNNNNNNNNNNN",
        "PPPPPPPPPPPPPPPP",
        "FFFFFFFFFFFFFFFF",
        "PPPPPPPPPPPPPPPP",
        "NNNNNNNNNNNNNNNN",
        "LHHHLHHHLHHHLHHH",
        "HDDDHDHDDHDDDHDH",
        "HDDDHDHDDHDDDHDH",
        "NNNNNNNNNNNNNNNN",
        "HLHHHLHHHLHHHLHH",
        "HDHDDDHDHDDDHDHD",
        "HDHDDDHDHDDDHDHD",
        "SSSSSSSSSSSSSSSS",
    ]
    return paint(
        rows,
        {"L": BR_L, "H": BR, "D": BR_D, "N": MOR, "S": MOR, "P": PIPE, "F": SHINE},
    )


def door() -> list:
    rows = [
        "................",
        "................",
        "................",
        "................",
        "................",
        "LLLLLLLLLLLLLLLL",
        "WWWWWWWWWWWWWWWW",
        "LLLLLLLLLLLLLLLL",
        "DDDDDDDDDDDDDDDD",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]
    return paint(rows, {".": _, "L": WOOD_L, "W": WOOD, "D": MOR})


def _fruits() -> list[tuple[str, list]]:
    """Sprite bonus, in ordine di livello. Stessa scala punti del labirinto."""
    G = (28, 118, 36, 255)
    L = (150, 206, 42, 255)
    S = (96, 64, 28, 255)
    specs = [
        (
            "ciliegie",
            [
                "....llll...lll..",
                "....lgggl.lggl..",
                ".....llgggggl...",
                ".......ggggg....",
                "......gg..g.....",
                ".....gg...g.....",
                "...ddd....g.....",
                "..dhhhdddddd....",
                ".ddhhhhddddddd..",
                ".dhhhhhhhhhhdd..",
                ".dHhhhhdhhhhhhd.",
                ".dHhhhhdHhhhhhd.",
                ".dhhhhdhdhhhhhd.",
                "..dhhhdddhhhhhd.",
                "...ddddddhhhhd..",
                "........ddddd...",
            ],
            {
                ".": _,
                "g": (22, 108, 32, 255),
                "l": (132, 198, 36, 255),
                "d": (128, 16, 40, 255),
                "h": (196, 28, 64, 255),
                "H": (248, 96, 120, 255),
            },
        ),
        (
            "fragola",
            [
                ".......g........",
                ".......g........",
                "......ggg.......",
                "....ggglggg.....",
                "...ggglglggg....",
                "..rrrgrrgrrrr...",
                "..rhrkrhrkrhrd..",
                "..rhrrrrrrrrrd..",
                "..rrkrhrrkrhrd..",
                "..rhrrkrhrrrrd..",
                "..rrhrrkrhrrrd..",
                "..rhrrrrkrhrrd..",
                "...rrkrhrrrd....",
                "....rrhrrd......",
                ".....rdddr......",
                "......rrrr......",
            ],
            {
                ".": _,
                "g": (24, 112, 36, 255),
                "l": (86, 168, 48, 255),
                "r": (236, 36, 64, 255),
                "h": (255, 128, 148, 255),
                "k": (140, 16, 44, 255),
                "d": (160, 20, 48, 255),
            },
        ),
        (
            "arancia",
            [
                "...gllg.........",
                "...glgl.........",
                "....glg.........",
                "....ggg.........",
                ".....gg.........",
                ".......g........",
                ".....oooooo.....",
                "....ohhhhhho....",
                "...ohhhhhhhho...",
                "...ohHhhhhhho...",
                "...ohHhhhhhho...",
                "...ohHhhhhhho...",
                "...ohhhhhhhho...",
                "...ohhhhhhhho...",
                "....ohhhhho.....",
                ".....ooooo......",
            ],
            {
                ".": _,
                "g": (24, 112, 32, 255),
                "l": (150, 206, 40, 255),
                "o": (196, 96, 16, 255),
                "h": (255, 148, 32, 255),
                "H": (255, 206, 120, 255),
            },
        ),
        (
            "mela",
            [
                ".........glg....",
                "........glllg...",
                ".......gllll....",
                ".......glgl.....",
                ".......ggg......",
                ".......s........",
                "...ddd.rrrr.....",
                "..dhhhhrhhhhd...",
                ".dhhhhhhhhhhrd..",
                ".dhhhrhhhhhhrd..",
                ".dhhhrrrrrrrrd..",
                ".drrrrrrrrrrrd..",
                ".drrrrrrrrrrrd..",
                "..drrrrrrrrrd...",
                "...ddrrrrrrd....",
                "....ddd.ddd.....",
            ],
            {
                ".": _,
                "g": G,
                "l": L,
                "s": S,
                "h": (255, 156, 172, 255),
                "r": (236, 40, 68, 255),
                "d": (150, 18, 48, 255),
            },
        ),
        (
            "anguria",
            [
                "................",
                "................",
                ".ddffffffffffdd.",
                "dlldffffffffdlld",
                "dggdffffffffdggd",
                "dggdfkffkffkdggd",
                "dggdffffffffdggd",
                "dggdffffffffdggd",
                ".dlggggggggggld.",
                ".ddlllllllllldd.",
                "..dggggggggggd..",
                "....dddddddd....",
                "................",
                "................",
                "................",
                "................",
            ],
            {
                ".": _,
                "d": (12, 78, 24, 255),
                "g": (22, 110, 34, 255),
                "l": (92, 176, 52, 255),
                "f": (240, 52, 78, 255),
                "k": (28, 14, 18, 255),
            },
        ),
        (
            "uva",
            [
                ".....s.sss......",
                "....ssssss......",
                "...ss.ss..ss....",
                "......pppp......",
                "...ppphppppp....",
                "..pphhppphpp....",
                "..phppppphppp...",
                "..phpppphpppp...",
                "...ppppppppp....",
                "...pphppphpp....",
                "...pphppphpp....",
                "...ppppppp......",
                ".....pphp.......",
                ".....pppp.......",
                ".....pppp.......",
                "......ppp.......",
            ],
            {
                ".": _,
                "s": (110, 68, 28, 255),
                "p": (118, 62, 168, 255),
                "h": (186, 140, 220, 255),
            },
        ),
        (
            "limone",
            [
                "........gllg....",
                "........glgg....",
                ".........glg....",
                "..........gg....",
                ".....yyyyyyl....",
                "....yyyyyyygg...",
                "...yyhhhhhhy....",
                "..yhhhhhhhhhy...",
                ".yhhhhhhhhhhhy..",
                ".yhhhyhhhhhhhy..",
                ".yhhyyhhhhhhhy..",
                ".yhhyyhhhhhhhy..",
                ".yhhhyhhhhhhyy..",
                ".yhhhhyhhhyy....",
                "..yhhhhhyy......",
                "...yyyyy........",
            ],
            {
                ".": _,
                "g": (24, 112, 32, 255),
                "l": (140, 196, 40, 255),
                "y": (196, 160, 28, 255),
                "h": (255, 228, 56, 255),
            },
        ),
        (
            "banana",
            [
                "................",
                "................",
                "...........sst..",
                "..........ysstt.",
                ".yyy.....ysssst.",
                "yhhhhhhhhyyyyst.",
                "yhhhyyhhhhyyystt",
                "yhhhhhhhhhhhyytt",
                ".yyyhhhhhhhhhhhh",
                "..yyhhhhhhhhhhh.",
                "...yhhhhhhhhyyt.",
                "....yhhhhhyyt...",
                ".....yyyyyy.....",
                "................",
                "................",
                "................",
            ],
            {
                ".": _,
                "h": (255, 220, 64, 255),
                "y": (230, 176, 36, 255),
                "s": (186, 124, 28, 255),
                "t": (110, 64, 22, 255),
            },
        ),
        (
            "ananas",
            [
                "......glg.......",
                ".....glglg......",
                "....gllgllg.....",
                ".....glglg......",
                "....gllgllg.....",
                ".....ggggg......",
                "....bybybyb.....",
                "...byyyyyyyb....",
                "...bybybybyb....",
                "...byyyyyyyb....",
                "...bybybybyb....",
                "...byyyyyyyb....",
                "...bybybybyb....",
                "...byyyyyyyb....",
                "....bybybyb.....",
                ".....bbbbb......",
            ],
            {
                ".": _,
                "g": (20, 100, 30, 255),
                "l": (72, 160, 44, 255),
                "b": (160, 100, 20, 255),
                "y": (255, 196, 40, 255),
            },
        ),
    ]
    out = []
    for name, rows, table in specs:
        out.append((name, paint(rows, table)))
    return out


def c_array(name: str, pixels: list, w: int, h: int) -> str:
    vals = [f"0x{rgb565(p):04X}" for p in pixels]
    lines = [f"static const uint16_t {name}[{w * h}] = {{"]
    for y in range(h):
        chunk = vals[y * w : (y + 1) * w]
        lines.append("    " + ", ".join(chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def load_png_rgba(path: Path) -> tuple[int, int, list[bytes]]:
    data = path.read_bytes()
    i = 8
    idat = b""
    w = h = None
    while i < len(data):
        ln = struct.unpack(">I", data[i : i + 4])[0]
        i += 4
        tag = data[i : i + 4]
        i += 4
        chunk = data[i : i + ln]
        i += ln + 4
        if tag == b"IHDR":
            w, h, bit, ctype, comp, filt, inter = struct.unpack(">IIBBBBB", chunk)
            if ctype != 6 or bit != 8:
                raise SystemExit(f"PNG non RGBA8: {path}")
        elif tag == b"IDAT":
            idat += chunk
        elif tag == b"IEND":
            break
    raw = zlib.decompress(idat)
    bpp = 4
    stride = w * bpp

    def paeth(a: int, b: int, c: int) -> int:
        p = a + b - c
        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
        if pa <= pb and pa <= pc:
            return a
        if pb <= pc:
            return b
        return c

    rows: list[bytes] = []
    prev = bytearray(stride)
    o = 0
    for _y in range(h):
        f = raw[o]
        o += 1
        cur = bytearray(raw[o : o + stride])
        o += stride
        if f == 1:
            for x in range(bpp, stride):
                cur[x] = (cur[x] + cur[x - bpp]) & 255
        elif f == 2:
            for x in range(stride):
                cur[x] = (cur[x] + prev[x]) & 255
        elif f == 3:
            for x in range(stride):
                left = cur[x - bpp] if x >= bpp else 0
                cur[x] = (cur[x] + ((left + prev[x]) // 2)) & 255
        elif f == 4:
            for x in range(stride):
                a = cur[x - bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x - bpp] if x >= bpp else 0
                cur[x] = (cur[x] + paeth(a, b, c)) & 255
        elif f != 0:
            raise SystemExit(f"filtro PNG {f}")
        rows.append(bytes(cur))
        prev = cur
    return w, h, rows


def sheet_cell(rows: list[bytes], col1: int, row1: int) -> list:
    c, r = col1 - 1, row1 - 1
    pix = []
    for y in range(32):
        line = rows[r * 32 + y]
        for x in range(32):
            o = (c * 32 + x) * 4
            pix.append(tuple(line[o : o + 4]))
    return pix


def center_in_square(pix: list, src_w: int, src_h: int, dst: int) -> list:
    """Centra il contenuto opaco in un quadrato dst x dst."""
    xs = [x for y in range(src_h) for x in range(src_w) if pix[y * src_w + x][3] >= 128]
    ys = [y for y in range(src_h) for x in range(src_w) if pix[y * src_w + x][3] >= 128]
    out = [_] * (dst * dst)
    if not xs:
        return out
    x0, x1 = min(xs), max(xs)
    y0, y1 = min(ys), max(ys)
    bw, bh = x1 - x0 + 1, y1 - y0 + 1
    ox = (dst - bw) // 2
    oy = (dst - bh) // 2
    for y in range(bh):
        for x in range(bw):
            r, g, b, a = pix[(y0 + y) * src_w + (x0 + x)]
            if a < 128:
                continue
            dx, dy = ox + x, oy + y
            if 0 <= dx < dst and 0 <= dy < dst:
                out[dy * dst + dx] = (r, g, b, 255)
    return out


def prep_cat_cell(pix32: list) -> list:
    """Cella sheet 32x32 centrata (resta 32x32, dettaglio pieno)."""
    return center_in_square(pix32, 32, 32, 32)


def recolor_cat(pix: list, target: tuple[int, int, int], sleep: bool = False) -> list:
    tr, tg, tb = target
    if sleep:
        tr, tg, tb = 104, 124, 176
    out = []
    for r, g, b, a in pix:
        if a < 128:
            out.append(_)
            continue
        bri = r + g + b
        if bri < 50:
            out.append(N)
            continue
        # occhi verdi / chiari: lascia (o spegni se sonno)
        if g > r + 15 and g > b + 15 and g > 80:
            if sleep:
                out.append(N)
            else:
                out.append((r, g, b, 255))
            continue
        # naso rosa/rosso
        if r > 160 and g < 100 and b < 100:
            out.append((211, 60, 56, 255) if not sleep else (80, 90, 140, 255))
            continue
        # pelliccia: scala luminosita' verso il target
        lum = bri / (3 * 255)
        # strisce scure restano piu' scure
        shade = 0.45 + lum * 0.85
        nr = min(255, int(tr * shade))
        ng = min(255, int(tg * shade))
        nb = min(255, int(tb * shade))
        out.append((nr, ng, nb, 255))
    return out


def load_sheet_cats() -> dict[str, list]:
    """dir -> [frame0, frame1, frame2] ciascuno 32x32 RGBA dallo sheet."""
    if not SHEET.exists():
        raise SystemExit(f"manca lo sheet gatti: {SHEET}")
    _w, _h, rows = load_png_rgba(SHEET)
    if _w != 384 or _h != 256:
        raise SystemExit(f"sheet inatteso {_w}x{_h}, atteso 384x256")
    out: dict[str, list] = {}
    for dname, row in SHEET_DIRS:
        frames = []
        for col in SHEET_COLS:
            frames.append(prep_cat_cell(sheet_cell(rows, col, row)))
        out[dname] = frames
    return out


def crop_rect(pix: list, sw: int, x: int, y: int, cw: int, ch: int) -> list:
    out = []
    for yy in range(ch):
        for xx in range(cw):
            out.append(pix[(y + yy) * sw + (x + xx)])
    return out


def fit_canvas(pix: list, sw: int, sh: int, dst: int) -> list:
    """Centra un rettangolo in un quadrato dst, senza scalare."""
    out = [_] * (dst * dst)
    ox = (dst - sw) // 2
    oy = (dst - sh) // 2
    for y in range(sh):
        for x in range(sw):
            r, g, b, a = pix[y * sw + x]
            if a < 128:
                continue
            dx, dy = ox + x, oy + y
            if 0 <= dx < dst and 0 <= dy < dst:
                out[dy * dst + dx] = (r, g, b, 255)
    return out


def mouse_cell(rows: list[bytes], col: int, y0: int, y1: int) -> list:
    pix = []
    x0 = col * MOUSE_CELL
    for y in range(y0, y1):
        line = rows[y]
        for x in range(x0, x0 + MOUSE_CELL):
            pix.append(tuple(line[x * 4 : x * 4 + 4]))
    return pix


def take_mouse_frame(cell: list, slice_h: int, canvas: int) -> list:
    if canvas < MOUSE_CELL or canvas < slice_h:
        cw = min(canvas, MOUSE_CELL)
        ch = min(canvas, slice_h)
        ox = (MOUSE_CELL - cw) // 2
        oy = (slice_h - ch) // 2
        cropped = crop_rect(cell, MOUSE_CELL, ox, oy, cw, ch)
        return fit_canvas(cropped, cw, ch, canvas)
    return fit_canvas(cell, MOUSE_CELL, slice_h, canvas)


def load_mouse_sheet() -> dict[str, list]:
    """nome banda -> 4 frame RGBA. Camminata 32x32, scatto e attacco 48x48."""
    if not MOUSE_SHEET.exists():
        raise SystemExit(f"manca lo sheet topo: {MOUSE_SHEET}")
    sw, sh, rows = load_png_rgba(MOUSE_SHEET)
    if sw != 192 or sh != 432:
        raise SystemExit(f"sheet topo inatteso {sw}x{sh}, atteso 192x432")
    out: dict[str, list] = {}
    for name, y0, y1, canvas in MOUSE_BANDS:
        slice_h = y1 - y0
        frames = []
        for col in range(4):
            cell = mouse_cell(rows, col, y0, y1)
            frames.append(take_mouse_frame(cell, slice_h, canvas))
        out[name] = frames
    return out


def mouse_life(frame32: list) -> list:
    """Icona vite: centro 16x16 del topo frontale."""
    return crop_rect(frame32, 32, 8, 8, 16, 16)


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    assets: list[tuple[str, int, int, list]] = []

    mouse = load_mouse_sheet()
    for dname in ("destra", "giu", "su"):
        for i, fr in enumerate(mouse[dname]):
            assets.append((f"topo_{dname}{i}", 32, 32, fr))
    for i, fr in enumerate(mouse["destra"]):
        assets.append((f"topo_sinistra{i}", 32, 32, flip_x(fr, 32, 32)))
    assets.append(("vita", 16, 16, mouse_life(mouse["giu"][0])))

    # Scatto e attacco: estratti, non ancora usati dal gioco.
    for kind in ("corsa", "attacco"):
        for dname in ("destra", "giu", "su"):
            for i, fr in enumerate(mouse[f"{kind}_{dname}"]):
                save_png(OUT_DIR / f"topo_{kind}_{dname}{i}.png", 48, 48, fr)
        for i, fr in enumerate(mouse[f"{kind}_destra"]):
            save_png(OUT_DIR / f"topo_{kind}_sinistra{i}.png", 48, 48, flip_x(fr, 48, 48))

    sheet = load_sheet_cats()
    for cname, target in CAT_TARGETS.items():
        for dname, frames in sheet.items():
            for fi, fr in enumerate(frames):
                assets.append(
                    (f"gatto_{cname}_{dname}{fi}", 32, 32, recolor_cat(fr, target))
                )
    assets.append(
        ("gatto_sonno", 32, 32, recolor_cat(sheet["giu"][1], CAT_TARGETS["marrone"], sleep=True))
    )
    assets.append(("gatto_occhi", 16, 16, cat_eyes()))

    assets += [
        ("formaggio", 16, 16, cheese()),
        ("ciotola", 16, 16, bottle()),
        ("porta", 16, 16, door()),
        ("muro", 16, 16, wall()),
        ("muro_tubo", 16, 16, wall_pipe()),
    ]
    for fname, pix in _fruits():
        assets.append((f"frutto_{fname}", 16, 16, pix))

    parts = [
        "/* Generato da tools/gen_maze_sprites.py — non editare a mano. */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "#define SPR_W 16",
        "#define SPR_H 16",
        "#define MOUSE_SPR 32",
        "#define MOUSE_FRAMES 4",
        "#define CAT_SPR 32",
        "#define CAT_FRAMES 3",
        "",
    ]
    for name, w, h, pix in assets:
        save_png(OUT_DIR / f"{name}.png", w, h, pix)
        parts.append(c_array(f"k_{name}", pix, w, h))
        parts.append("")

    HDR.write_text("\n".join(parts) + "\n", encoding="utf-8")
    print(f"scritti {len(assets)} sprite in {OUT_DIR} e {HDR}")


if __name__ == "__main__":
    main()
