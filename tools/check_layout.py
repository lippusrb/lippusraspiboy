#!/usr/bin/env python3
"""Controlla che layout.h, game.ld e CMakeLists raccontino la stessa mappa."""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def grab(text: str, pattern: str) -> int:
    match = re.search(pattern, text)
    if not match:
        raise SystemExit(f"manca {pattern}")
    return int(match.group(1), 16)


def main() -> None:
    header = (ROOT / "sdk/include/lippus/layout.h").read_text()
    linker = (ROOT / "sdk/linker/game.ld").read_text()
    os_ld = (ROOT / "src/memmap_lippus.ld").read_text()
    pairs = [
        ("LIPPUS_OS_FLASH_LIMIT", "OS_FLASH_LIMIT"),
        ("LIPPUS_OS_RAM_LENGTH", "OS_RAM_LENGTH"),
        ("LIPPUS_FLASH_BYTES", "FLASH_SIZE"),
        ("LIPPUS_RAM_ORIGIN", "SRAM_ORIGIN"),
        ("LIPPUS_SRAM_END", "SRAM_END"),
        ("LIPPUS_INSTALL_RESERVE", "INSTALL_RESERVE"),
    ]
    values = {}
    for c_name, ld_name in pairs:
        c_val = grab(header, rf"#define\s+{c_name}\s+(0x[0-9A-Fa-f]+)u?")
        ld_val = grab(linker, rf"{ld_name}\s*=\s*(0x[0-9A-Fa-f]+)\s*;")
        if c_val != ld_val:
            raise SystemExit(f"{c_name}={c_val:#x} diverso da {ld_name}={ld_val:#x}")
        values[c_name] = c_val
    ram_len = grab(os_ld, r"RAM\(rwx\)\s*:\s*ORIGIN\s*=\s*0x20000000\s*,\s*LENGTH\s*=\s*(0x[0-9A-Fa-f]+)")
    if ram_len != values["LIPPUS_OS_RAM_LENGTH"]:
        raise SystemExit("LENGTH RAM in memmap_lippus.ld non coincide con layout.h")
    expect_heap = values["LIPPUS_RAM_ORIGIN"] + values["LIPPUS_OS_RAM_LENGTH"]
    if values["LIPPUS_SRAM_END"] <= expect_heap:
        raise SystemExit("la RAM dei giochi e' vuota")
    print("layout allineato")


if __name__ == "__main__":
    main()
