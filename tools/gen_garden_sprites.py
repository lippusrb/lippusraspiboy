#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Sprite puliti a pixel per Garden Defender (PNG + sprites.h).

Le immagini stock JPEG non conservano una griglia pixel-art utilizzabile:
qui i disegni sono rifatti a mano, ispirati a quelle reference.
"""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "games" / "garden_defender" / "sprites"
HDR = ROOT / "games" / "garden_defender" / "sprites.h"

_ = None  # trasparente -> magenta key in C
# palette
N = (16, 12, 10)       # nero contorno
W = (255, 255, 255)
R = (220, 40, 40)      # rosso coccinella / rosa
Rd = (140, 20, 20)
Pk = (255, 120, 160)   # tulipano
Pd = (200, 60, 110)
Ye = (255, 220, 48)
Gn = (56, 180, 64)
Gd = (30, 120, 40)
Gl = (120, 220, 100)
Br = (186, 96, 52)     # terracotta
Bd = (130, 62, 36)
Bl = (220, 140, 90)
Mo = (60, 40, 36)      # zanzara
Mw = (200, 220, 230)   # ali
Mg = (80, 170, 70)     # mantide
Md = (40, 110, 50)
Bu = (40, 90, 220)     # farfalla
By = (255, 220, 80)
Bc = (80, 200, 220)


def save_png(path: Path, w: int, h: int, pixels: list) -> None:
    raw = b""
    for y in range(h):
        raw += b"\x00"
        for x in range(w):
            p = pixels[y * w + x]
            if p is None:
                raw += bytes((0, 0, 0, 0))
            else:
                raw += bytes((p[0], p[1], p[2], 255))

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 9))
    out += chunk(b"IEND", b"")
    path.write_bytes(out)


def paint(rows: list[str], table: dict) -> tuple[int, int, list]:
    h = len(rows)
    w = len(rows[0])
    for i, row in enumerate(rows):
        if len(row) != w:
            raise SystemExit(f"riga {i} len {len(row)} != {w}: {row!r}")
    pix = []
    for row in rows:
        for ch in row:
            pix.append(table[ch])
    return w, h, pix


def rgb565(p) -> int:
    if p is None:
        return 0xF81F
    r, g, b = p
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def c_array(name: str, w: int, h: int, pix: list) -> str:
    vals = [rgb565(p) for p in pix]
    lines = [f"static const uint16_t {name}[{w * h}] = {{"]
    for y in range(h):
        chunk = vals[y * w : (y + 1) * w]
        lines.append("    " + ", ".join(f"0x{v:04X}" for v in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def ladybug_8():
    # coccinella vista dall'alto
    rows = [
        "..NN....",
        ".NRRN...",
        "NRRdRRN.",
        "NRRRdRN.",
        "NRRdRRN.",
        ".NRRRN..",
        "..N.N...",
        "........",
    ]
    return paint(rows, {".": _, "N": N, "R": R, "d": Rd})


def mosquito_16():
    rows = [
        "................",
        ".......N........",
        "......NNN.......",
        ".....NMMMN......",
        "...AANMMMNAA....",
        "..AA.NMMMN.AA...",
        ".AA..NMMMN..AA..",
        ".....NMMMN......",
        "....N.MMM.N.....",
        "...N..MMM..N....",
        "..N...N.N...N...",
        ".N....N.N....N..",
        "......N.N.......",
        ".......N........",
        "................",
        "................",
    ]
    return paint(rows, {".": _, "N": N, "M": Mo, "A": Mw})


def mantis_16():
    rows = [
        "................",
        "......G.G.......",
        ".....NGNGN......",
        "......NGN.......",
        "...L..NGN..L....",
        "..LL.NGGGN.LL...",
        ".....NGGGN......",
        ".....NGGGN......",
        "....NGGGGGN.....",
        "....NGGdGGN.....",
        "....NGGGGGN.....",
        "...N..NNN..N....",
        "...N.......N....",
        "................",
        "................",
        "................",
    ]
    return paint(rows, {".": _, "N": N, "G": Mg, "d": Md, "L": Gl})


def butterfly_16():
    rows = [
        "................",
        ".......N.N......",
        "......N...N.....",
        "..BBBN.....NBBB.",
        ".BYYBN.....NYYB.",
        ".BYCBN.NNN.NCBY.",
        ".BCCBN.NNN.NCCB.",
        "..BCBN.....NBCB.",
        "...BBN.....NBB..",
        "....BN.NNN.NB...",
        ".....N.....N....",
        "................",
        "................",
        "................",
        "................",
        "................",
    ]
    return paint(rows, {".": _, "N": N, "B": Bu, "Y": By, "C": Bc})


def pot_base_bottom() -> list[str]:
    # 24 colonne esatte, vaso terracotta (B chiaro, C medio, D scuro)
    rows = [
        "...NNNNNNNNNNNNNNNNNN...",  # 24
        "..NCCCCCCCCCCCCCCCCCCNN.",  # 24
        ".NCCBBBBBBBBBBBBBBBBDCN.",  # 24
        ".NCBBBBBBBBBBBBBBBBBDCN.",  # 24
        ".NCCCCCCCCCCCCCCCCCCDCN.",  # 24
        "..NCCCCCCCCCCCCCCCCDCN..",  # 24
        "...NCCCCCCCCCCCCCCDCN...",  # 24
        "....NCCCCCCCCCCCCDCN....",  # 24
        ".....NNNNNNNNNNNNNN.....",  # 24
    ]
    for i, r in enumerate(rows):
        if len(r) != 24:
            raise SystemExit(f"pot base {i} len {len(r)}: {r!r}")
    return rows


def vaso_tulipano():
    # 24x32: fiore rosa + foglie + vaso
    top = [
        "........................",
        "..........NN............",
        ".........NPPN...........",
        "........NPPPPdN.........",
        ".......NPPPPPPN.........",
        "......NPPPdPPPN.........",
        "......NPPPPPPPN.........",
        ".......NPPPPPN..........",
        "........NPPPN...........",
        ".........NGN............",
        "........NGGGN...........",
        ".......NGN.NGN..........",
        "......NGN...NGN.........",
        ".....NGN.....NGN........",
        "......N.......N.........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        ".........NNNN...........",
    ]
    rows = top + pot_base_bottom()
    # pad/trim to 32
    while len(rows) < 32:
        rows.append("........................")
    rows = rows[:32]
    return paint(
        rows,
        {
            ".": _,
            "N": N,
            "P": Pk,
            "d": Pd,
            "G": Gn,
            "B": Bl,
            "C": Br,
            "D": Bd,
        },
    )


def vaso_margherita():
    top = [
        "........................",
        ".........NWNWN..........",
        ".......NWWNYNWNW........",
        "......NWWYNYYNWWWN......",
        ".......NWWNYNWNW........",
        ".........NWNWN..........",
        "..........NGN...........",
        "........NGN.NGN.........",
        ".......NGN...NGN........",
        "......NGN.....NGN.......",
        ".......N.......N........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        ".........NNNN...........",
    ]
    rows = top + pot_base_bottom()
    while len(rows) < 32:
        rows.append("........................")
    rows = rows[:32]
    return paint(
        rows,
        {
            ".": _,
            "N": N,
            "W": W,
            "Y": Ye,
            "G": Gn,
            "B": Bl,
            "C": Br,
            "D": Bd,
        },
    )


def vaso_rosa():
    top = [
        "........................",
        "..........NN............",
        ".........NRRN...........",
        "........NRRdRN..........",
        ".......NRRRRRN..........",
        "......NRRRdRRRN.........",
        "......NRRRRRRRN.........",
        ".......NRRRRRN..........",
        "........NRRRN...........",
        ".........NGN............",
        ".......NGN.NGN..........",
        "......NGN...NGN.........",
        ".....NGN.....NGN........",
        "......N.......N.........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        "..........NGN...........",
        ".........NNNN...........",
    ]
    rows = top + pot_base_bottom()
    while len(rows) < 32:
        rows.append("........................")
    rows = rows[:32]
    return paint(
        rows,
        {
            ".": _,
            "N": N,
            "R": R,
            "d": Rd,
            "G": Gn,
            "B": Bl,
            "C": Br,
            "D": Bd,
        },
    )


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    assets = []

    w, h, pix = ladybug_8()
    assets.append(("afide", w, h, pix))
    w, h, pix = mosquito_16()
    assets.append(("zanzara", w, h, pix))
    w, h, pix = mantis_16()
    assets.append(("cavalletta", w, h, pix))
    w, h, pix = butterfly_16()
    assets.append(("farfalla", w, h, pix))

    for name, fn in (
        ("vaso0", vaso_tulipano),
        ("vaso1", vaso_margherita),
        ("vaso2", vaso_rosa),
    ):
        w, h, pix = fn()
        assets.append((name, w, h, pix))

    parts = [
        "/* Generato da tools/gen_garden_sprites.py — pixel art a mano. */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "#define BUG8_W 8",
        "#define BUG8_H 8",
        "#define BUG16_W 16",
        "#define BUG16_H 16",
        "#define POT_SPR_W 24",
        "#define POT_SPR_H 32",
        "",
    ]

    for name, w, h, pix in assets:
        save_png(OUT / f"{name}.png", w, h, pix)
        # alias utili
        if name == "afide":
            save_png(OUT / "afide_8.png", w, h, pix)
        elif name in ("zanzara", "cavalletta", "farfalla"):
            save_png(OUT / f"{name}_16.png", w, h, pix)
        parts.append(c_array(f"k_{name}", w, h, pix))
        parts.append("")

    parts.append("static const uint16_t *const k_vasi[3] = {k_vaso0, k_vaso1, k_vaso2};")
    parts.append("")
    HDR.write_text("\n".join(parts) + "\n", encoding="utf-8")
    print(f"scritti {len(assets)} sprite -> {OUT} e {HDR}")


if __name__ == "__main__":
    main()
