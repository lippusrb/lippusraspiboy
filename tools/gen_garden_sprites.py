#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Impacchetta gli sprite PNG di Garden Defender in sprites.h (RGB565)."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "games" / "garden_defender" / "sprites"
HDR = ROOT / "games" / "garden_defender" / "sprites.h"

ASSETS = [
    ("bug1_1", 16, 16),
    ("bug1_2", 16, 16),
    ("bug2_1", 16, 16),
    ("bug2_2", 16, 16),
    ("bug3_1", 16, 16),
    ("bug3_2", 16, 16),
    ("bug4_1", 16, 16),
    ("bug4_2", 16, 16),
    ("bug5_1", 8, 8),
    ("bug5_2", 8, 8),
    ("farfalla1", 16, 16),
    ("farfalla2", 16, 16),
    ("vaso", 32, 32),
    ("spray1", 24, 24),
    ("spray2", 24, 24),
    ("goccia", 8, 8),
]


def load_png_rgba(path: Path) -> tuple[int, int, list[tuple[int, int, int, int]]]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"non e' un PNG: {path}")
    i = 8
    idat = b""
    w = h = bit = ct = None
    while i < len(data):
        ln = struct.unpack(">I", data[i : i + 4])[0]
        tag = data[i + 4 : i + 8]
        chunk = data[i + 8 : i + 8 + ln]
        if tag == b"IHDR":
            w, h, bit, ct = struct.unpack(">IIBB", chunk[:10])
        elif tag == b"IDAT":
            idat += chunk
        elif tag == b"IEND":
            break
        i += 12 + ln
    if w is None or bit != 8 or ct not in (2, 6):
        raise SystemExit(f"PNG non supportato {path}: {w}x{h} bit={bit} ct={ct}")
    raw = zlib.decompress(idat)
    bpp = 4 if ct == 6 else 3
    stride = w * bpp
    rows: list[bytes] = []
    prev = bytearray(stride)
    p = 0

    def paeth(a: int, b: int, c: int) -> int:
        pp = a + b - c
        pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
        if pa <= pb and pa <= pc:
            return a
        if pb <= pc:
            return b
        return c

    for _y in range(h):
        filt = raw[p]
        p += 1
        row = bytearray(raw[p : p + stride])
        p += stride
        if filt == 1:
            for x in range(stride):
                if x >= bpp:
                    row[x] = (row[x] + row[x - bpp]) & 255
        elif filt == 2:
            for x in range(stride):
                row[x] = (row[x] + prev[x]) & 255
        elif filt == 3:
            for x in range(stride):
                a = row[x - bpp] if x >= bpp else 0
                row[x] = (row[x] + ((a + prev[x]) // 2)) & 255
        elif filt == 4:
            for x in range(stride):
                a = row[x - bpp] if x >= bpp else 0
                b = prev[x]
                c = prev[x - bpp] if x >= bpp else 0
                row[x] = (row[x] + paeth(a, b, c)) & 255
        elif filt != 0:
            raise SystemExit(f"filtro {filt} in {path}")
        rows.append(bytes(row))
        prev = row

    pix: list[tuple[int, int, int, int]] = []
    for y in range(h):
        row = rows[y]
        for x in range(w):
            o = x * bpp
            if bpp == 4:
                pix.append((row[o], row[o + 1], row[o + 2], row[o + 3]))
            else:
                pix.append((row[o], row[o + 1], row[o + 2], 255))
    return w, h, pix


def rgb565(px: tuple[int, int, int, int]) -> int:
    r, g, b, a = px
    if a < 128:
        return 0xF81F
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def c_array(name: str, w: int, h: int, pix: list[tuple[int, int, int, int]]) -> str:
    vals = [f"0x{rgb565(p):04X}" for p in pix]
    lines = [f"static const uint16_t {name}[{w * h}] = {{"]
    for y in range(h):
        chunk = vals[y * w : (y + 1) * w]
        lines.append("    " + ", ".join(chunk) + ",")
    lines.append("};")
    return "\n".join(lines)


def save_png(path: Path, w: int, h: int, pixels: list[tuple[int, int, int, int]]) -> None:
    raw = b""
    for y in range(h):
        raw += b"\x00"
        for x in range(w):
            r, g, b, a = pixels[y * w + x]
            raw += bytes((r, g, b, a))

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    out = b"\x89PNG\r\n\x1a\n"
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    out += chunk(b"IDAT", zlib.compress(raw, 9))
    out += chunk(b"IEND", b"")
    path.write_bytes(out)


def main() -> None:
    parts = [
        "/* Generato da tools/gen_garden_sprites.py — non editare a mano. */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "#define BUG8_W 8",
        "#define BUG8_H 8",
        "#define BUG16_W 16",
        "#define BUG16_H 16",
        "#define POT_SPR_W 32",
        "#define POT_SPR_H 32",
        "#define BUG_FRAMES 2",
        "#define PLAYER_SPR_W 24",
        "#define PLAYER_SPR_H 24",
        "#define SHOT_SPR_W 8",
        "#define SHOT_SPR_H 8",
        "",
    ]
    for name, ew, eh in ASSETS:
        path = OUT / f"{name}.png"
        if not path.exists():
            raise SystemExit(f"manca lo sprite {path}")
        w, h, pix = load_png_rgba(path)
        if w != ew or h != eh:
            raise SystemExit(f"{path.name}: {w}x{h}, atteso {ew}x{eh}")
        save_png(path, w, h, pix)
        parts.append(c_array(f"k_{name}", w, h, pix))
        parts.append("")

    parts.append("static const uint16_t *const k_bug1[BUG_FRAMES] = {k_bug1_1, k_bug1_2};")
    parts.append("static const uint16_t *const k_bug2[BUG_FRAMES] = {k_bug2_1, k_bug2_2};")
    parts.append("static const uint16_t *const k_bug3[BUG_FRAMES] = {k_bug3_1, k_bug3_2};")
    parts.append("static const uint16_t *const k_bug4[BUG_FRAMES] = {k_bug4_1, k_bug4_2};")
    parts.append("static const uint16_t *const k_bug5[BUG_FRAMES] = {k_bug5_1, k_bug5_2};")
    parts.append("static const uint16_t *const k_farfalla[BUG_FRAMES] = {k_farfalla1, k_farfalla2};")
    parts.append("static const uint16_t *const k_spray[BUG_FRAMES] = {k_spray1, k_spray2};")
    parts.append("static const uint16_t *const k_vasi[3] = {k_vaso, k_vaso, k_vaso};")
    parts.append("")
    HDR.write_text("\n".join(parts) + "\n", encoding="utf-8")
    print(f"scritti {len(ASSETS)} sprite -> {OUT} e {HDR}")


if __name__ == "__main__":
    main()
