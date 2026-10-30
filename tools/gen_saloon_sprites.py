#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PNG di games/saloon -> sprites.h in RGB565. Serve Pillow.

I PNG in games/saloon/sprites/ sono quelli da ritoccare a mano.
Senza argomenti lo script li impacchetta soltanto.
--prepare li rigenera da sprites/_src e cancella i ritocchi.

Lo sfondo e' il saloon 320x256 tagliato a 320x240 (via i 16 px di terra in basso).
Il magenta 0xF81F e' il colore trasparente degli sprite.
"""

from __future__ import annotations

from pathlib import Path
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
GAME = ROOT / "games" / "saloon"
SPR = GAME / "sprites"
HDR = GAME / "sprites.h"
BG = GAME / "sfondo.png"
KEY = 0xF81F

ACTORS = (
    "bandito_porta",
    "donna_porta",
    "uomo_porta",
    "bandito_passo",
    "donna_passo",
    "uomo_passo",
    "bandito_finestra",
    "donna_finestra",
    "uomo_finestra",
    "donna_arrabbiata",
    "uomo_arrabbiato",
    "bandito_colpito",
    "bandito_rinculo",
)

# Sportelli sul saloon 320x256, prima del taglio in basso.
LEAF_L = (139, 122, 160, 180)
LEAF_R = (161, 122, 182, 180)
LEAF_SCALES = (1.0, 0.74, 0.50, 0.28, 0.12)


def rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def pixels_of(im: Image.Image) -> tuple[int, int, list[int]]:
    rgba = im.convert("RGBA")
    w, h = rgba.size
    src = rgba.load()
    out: list[int] = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = src[x, y]
            if a < 128:
                out.append(KEY)
                continue
            color = rgb565(r, g, b)
            if color == KEY:
                color = KEY - 1
            out.append(color)
    return w, h, out


def is_frame(r: int, g: int, b: int, a: int) -> bool:
    if a < 40:
        return False
    if r < 90 or g < 35 or b > 140:
        return False
    if r < g - 5 or (r - b) < 25:
        return False
    if r > 155 and g < 90 and b < 80 and (r - g) > 60:
        return False
    return True


def is_void(r: int, g: int, b: int, a: int, lum_max: int) -> bool:
    if a < 40:
        return True
    lum = (r + g + b) / 3
    span = max(r, g, b) - min(r, g, b)
    return lum < lum_max and span < 40


def flood(im: Image.Image, pred, seeds, xlim=None) -> None:
    w, h = im.size
    px = im.load()
    seen = bytearray(w * h)
    stack = list(seeds)
    while stack:
        x, y = stack.pop()
        if x < 0 or y < 0 or x >= w or y >= h:
            continue
        if xlim is not None and not xlim(x):
            continue
        i = y * w + x
        if seen[i]:
            continue
        seen[i] = 1
        r, g, b, a = px[x, y]
        if not pred(r, g, b, a):
            continue
        px[x, y] = (0, 0, 0, 0)
        stack.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))


def edge_seeds(w: int, h: int):
    seeds = [(x, 0) for x in range(w)] + [(x, h - 1) for x in range(w)]
    seeds += [(0, y) for y in range(h)] + [(w - 1, y) for y in range(h)]
    return seeds


def drop_border_junk(im: Image.Image) -> Image.Image:
    """Toglie i pezzi di cornice rimasti sul bordo, staccati dal personaggio."""
    im = im.convert("RGBA")
    w, h = im.size
    px = im.load()
    seen = bytearray(w * h)
    junk = []
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if seen[i] or px[x, y][3] < 10:
                continue
            stack = [(x, y)]
            seen[i] = 1
            comp = []
            while stack:
                cx, cy = stack.pop()
                comp.append((cx, cy))
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if nx < 0 or ny < 0 or nx >= w or ny >= h:
                        continue
                    j = ny * w + nx
                    if not seen[j] and px[nx, ny][3] >= 10:
                        seen[j] = 1
                        stack.append((nx, ny))
            xs = [p[0] for p in comp]
            ys = [p[1] for p in comp]
            x0, x1 = min(xs), max(xs)
            y0, y1 = min(ys), max(ys)
            edge = False
            if y1 <= 2 and len(comp) < 80:
                edge = True
            if x1 <= 2 and len(comp) < 40:
                edge = True
            if x0 >= w - 3 and len(comp) < 40:
                edge = True
            if edge:
                junk.extend(comp)
    for x, y in junk:
        px[x, y] = (0, 0, 0, 0)
    return im


def crop_opaque(im: Image.Image) -> Image.Image:
    px = im.load()
    w, h = im.size
    xs = []
    ys = []
    for y in range(h):
        for x in range(w):
            if px[x, y][3] > 10:
                xs.append(x)
                ys.append(y)
    if not xs:
        return im
    return im.crop((min(xs), min(ys), max(xs) + 1, max(ys) + 1))


def peel(im: Image.Image, lum_max: int, sides: bool) -> Image.Image:
    """Toglie cornice e interno scuro, lascia il personaggio."""
    im = im.convert("RGBA")
    w, h = im.size
    seeds = edge_seeds(w, h)
    flood(im, is_frame, seeds)
    flood(im, lambda r, g, b, a: is_void(r, g, b, a, lum_max), seeds)
    if sides:
        margin = max(8, w // 5)
        px = im.load()

        def on_side(x: int) -> bool:
            return x < margin or x >= w - margin

        side_seeds = []
        for y in range(h):
            for x in range(w):
                if not on_side(x) or not is_frame(*px[x, y]):
                    continue
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if nx < 0 or ny < 0 or nx >= w or ny >= h or px[nx, ny][3] < 40:
                        side_seeds.append((x, y))
                        break
        flood(im, is_frame, side_seeds, xlim=on_side)
    if not sides:
        im = drop_border_junk(im)
    return crop_opaque(im)


def strip_opening(im: Image.Image, margin: int) -> Image.Image:
    """Toglie i listelli della porta ai lati, senza entrare nel corpo."""
    im = im.convert("RGBA")
    w, h = im.size

    def side(x: int) -> bool:
        return x < margin or x >= w - margin

    def frame_or_void(r: int, g: int, b: int, a: int) -> bool:
        if a < 40:
            return True
        if is_void(r, g, b, a, 50):
            return True
        if r < 70 or g < 28 or b > 150:
            return False
        if r < g - 8 or (r - b) < 18:
            return False
        if r > 160 and g < 90 and b < 80 and (r - g) > 55:
            return False
        return True

    seeds = edge_seeds(w, h)
    flood(im, lambda r, g, b, a: is_void(r, g, b, a, 50), seeds)
    px = im.load()
    for _ in range(8):
        side_seeds = []
        for y in range(h):
            for x in range(w):
                if not side(x) or px[x, y][3] < 40:
                    continue
                if not frame_or_void(*px[x, y]):
                    continue
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if nx < 0 or ny < 0 or nx >= w or ny >= h or px[nx, ny][3] < 40:
                        side_seeds.append((x, y))
                        break
        if not side_seeds:
            break
        flood(im, frame_or_void, side_seeds, xlim=side)
    return drop_top_bits(crop_opaque(im))


def drop_top_bits(im: Image.Image) -> Image.Image:
    """Toglie il listello staccato sopra la testa."""
    im = im.convert("RGBA")
    w, h = im.size
    px = im.load()
    seen = bytearray(w * h)
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if seen[i] or px[x, y][3] < 40:
                continue
            stack = [(x, y)]
            seen[i] = 1
            cells = []
            ymax = 0
            while stack:
                cx, cy = stack.pop()
                cells.append((cx, cy))
                if cy > ymax:
                    ymax = cy
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if nx < 0 or ny < 0 or nx >= w or ny >= h:
                        continue
                    j = ny * w + nx
                    if seen[j] or px[nx, ny][3] < 40:
                        continue
                    seen[j] = 1
                    stack.append((nx, ny))
            if ymax <= 4:
                for cx, cy in cells:
                    px[cx, cy] = (0, 0, 0, 0)
    return crop_opaque(im)


def drop_smoke(im: Image.Image) -> Image.Image:
    """Toglie la nuvola grigia del colpo, non la camicia."""
    im = im.convert("RGBA")
    w, h = im.size
    px = im.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 40:
                continue
            span = max(r, g, b) - min(r, g, b)
            lum = (r + g + b) / 3
            neutral = lum > 115 and span < 16 and abs(r - b) < 12
            cloud = x < int(w * 0.46) and lum > 165 and span < 70 and (r - b) < 70
            if neutral or cloud:
                px[x, y] = (0, 0, 0, 0)
    return crop_opaque(im)


def walk_frame(im: Image.Image) -> Image.Image:
    """Un passo: una gamba si alza, l'altra resta."""
    im = im.convert("RGBA")
    w, h = im.size
    px = im.load()
    ys = [y for y in range(h) if any(px[x, y][3] > 10 for x in range(w))]
    xs = [x for x in range(w) if any(px[x, y][3] > 10 for y in range(h))]
    y0, y1 = min(ys), max(ys)
    x0, x1 = min(xs), max(xs)
    leg = y0 + int((y1 - y0 + 1) * 0.70)
    mid = (x0 + x1) // 2
    out = im.copy()
    op = out.load()
    for y in range(leg, h):
        for x in range(w):
            op[x, y] = (0, 0, 0, 0)
    for y in range(leg, h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 10:
                continue
            left = x <= mid
            dx = 2 if left else -2
            dy = -2 if left else 0
            nx = x + dx
            ny = y + dy
            if 0 <= nx < w and 0 <= ny < h:
                op[nx, ny] = (r, g, b, a)
    return out


def key_leaf(im: Image.Image, side: str) -> Image.Image:
    im = im.convert("RGBA")
    w, h = im.size
    px = im.load()

    def doorish(r: int, g: int, b: int, a: int) -> bool:
        return a > 200 and r > 70 and g > 28 and r > b + 8

    seeds = [(0, 0), (w - 1, 0), (w // 2, 0)]
    if side == "L":
        seeds += [(w - 1, y) for y in range(0, h, 2)]
    else:
        seeds += [(0, y) for y in range(0, h, 2)]
    seen = bytearray(w * h)
    stack = list(seeds)
    while stack:
        x, y = stack.pop()
        if not (0 <= x < w and 0 <= y < h):
            continue
        i = y * w + x
        if seen[i]:
            continue
        seen[i] = 1
        r, g, b, a = px[x, y]
        if doorish(r, g, b, a):
            continue
        px[x, y] = (0, 0, 0, 0)
        stack.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    return im


def squash_leaf(im: Image.Image, scale: float, anchor: str) -> Image.Image:
    w, h = im.size
    nw = max(1, int(round(w * scale)))
    scaled = im.resize((nw, h), Image.NEAREST)
    canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    canvas.paste(scaled, (0 if anchor == "L" else w - nw, 0), scaled)
    return canvas


def prepare() -> None:
    """Pulisce i personaggi, fa il passo e ricava gli sportelli.

    Gli originali restano in sprites/_src, così si può rigenerare.
    """
    backup = SPR / "_src"
    backup.mkdir(exist_ok=True)
    for name, lum, sides in (
        ("bandito_finestra", 48, False),
        ("donna_finestra", 48, False),
        ("uomo_finestra", 48, False),
        ("bandito_porta", 34, True),
        ("donna_porta", 34, True),
        ("uomo_porta", 34, True),
    ):
        src = backup / f"{name}.png"
        if not src.exists():
            src.write_bytes((SPR / f"{name}.png").read_bytes())
        drop_top_bits(peel(Image.open(src), lum, sides)).save(SPR / f"{name}.png")
    for name, margin in (("bandito_porta", 16), ("donna_porta", 16)):
        src = backup / f"{name}.png"
        strip_opening(Image.open(src), margin).save(SPR / f"{name}.png")
    for name, margin in (
        ("bandito_colpito", 12),
        ("bandito_rinculo", 8),
        ("donna_arrabbiata", 12),
        ("uomo_arrabbiato", 6),
    ):
        src = backup / f"{name}.png"
        if not src.exists():
            src.write_bytes((SPR / f"{name}.png").read_bytes())
        cleaned = drop_smoke(strip_opening(Image.open(src), margin))
        cleaned.save(SPR / f"{name}.png")
    for name in ("bandito", "donna", "uomo"):
        src = Image.open(SPR / f"{name}_porta.png")
        walk_frame(src).save(SPR / f"{name}_passo.png")

    facade = Image.open(BG).convert("RGBA")
    left = key_leaf(facade.crop(LEAF_L), "L")
    right = key_leaf(facade.crop(LEAF_R), "R")
    hole = Image.new("RGBA", (LEAF_R[2] - LEAF_L[0], LEAF_L[3] - LEAF_L[1]), (0, 0, 0, 0))
    hp = hole.load()
    for src, ox in ((left, 0), (right, LEAF_R[0] - LEAF_L[0])):
        sp = src.load()
        for y in range(src.size[1]):
            for x in range(src.size[0]):
                if sp[x, y][3] > 10:
                    hp[ox + x, y] = (6, 3, 4, 255)
    hole.save(SPR / "anta_buco.png")
    for i, scale in enumerate(LEAF_SCALES):
        squash_leaf(left, scale, "L").save(SPR / f"anta_sx{i}.png")
        squash_leaf(right, scale, "R").save(SPR / f"anta_dx{i}.png")


def emit(out, name: str, w: int, h: int, data: list[int]) -> None:
    macro = "SPR_" + name.upper()
    out.write(f"#define {macro}_W {w}\n")
    out.write(f"#define {macro}_H {h}\n")
    out.write(f"static const uint16_t spr_{name}[{w * h}] = {{\n")
    line: list[str] = []
    for i, value in enumerate(data):
        line.append(f"0x{value:04X}")
        if len(line) == 12 or i + 1 == len(data):
            out.write("    " + ", ".join(line) + ",\n")
            line = []
    out.write("};\n\n")


def main() -> None:
    bg = Image.open(BG)
    if bg.size[0] < 320 or bg.size[1] < 240:
        raise SystemExit(f"sfondo troppo piccolo: {bg.size}")
    bg = bg.crop((0, 0, 320, 240))
    if "--prepare" in sys.argv:
        prepare()
    chunks = [("sfondo", pixels_of(bg))]
    for name in ACTORS:
        path = SPR / f"{name}.png"
        chunks.append((name, pixels_of(Image.open(path))))
    for i in range(len(LEAF_SCALES)):
        chunks.append((f"anta_sx{i}", pixels_of(Image.open(SPR / f"anta_sx{i}.png"))))
        chunks.append((f"anta_dx{i}", pixels_of(Image.open(SPR / f"anta_dx{i}.png"))))
    chunks.append(("anta_buco", pixels_of(Image.open(SPR / "anta_buco.png"))))

    with HDR.open("w", encoding="utf-8") as out:
        out.write("/* Generato da tools/gen_saloon_sprites.py — non editare a mano. */\n")
        out.write("#pragma once\n")
        out.write("#include <stdint.h>\n\n")
        out.write(f"#define ANTA_SX_X {LEAF_L[0]}\n")
        out.write(f"#define ANTA_DX_X {LEAF_R[0]}\n")
        out.write(f"#define ANTA_Y {LEAF_L[1]}\n")
        out.write(f"#define ANTA_N {len(LEAF_SCALES)}\n\n")
        for name, (w, h, data) in chunks:
            emit(out, name, w, h, data)
            print(f"{name} {w}x{h}")


if __name__ == "__main__":
    main()
