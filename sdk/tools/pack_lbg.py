#!/usr/bin/env python3
"""Impacchetta un ELF di gioco in un file .lbg per la SD di Lippus Boy."""

import argparse
import struct
import sys

API_VERSION = 2
HEADER_BYTES = 64
EXPECT_MAGIC = b"LBG1"
LBG_FLAG_PORTRAIT = 1


def crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xEDB88320 if crc & 1 else crc >> 1
    return crc ^ 0xFFFFFFFF


def elf32(data: bytes, path: str) -> None:
    if len(data) < 52 or data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise SystemExit(f"{path}: serve un ELF32 little-endian")


def read_elf_load(data: bytes, path: str) -> tuple[int, int]:
    elf32(data, path)
    entry, phoff = struct.unpack_from("<II", data, 24)
    phentsize, phnum = struct.unpack_from("<HH", data, 42)
    load = None
    for index in range(phnum):
        off = phoff + index * phentsize
        p_type, _, _, paddr, filesz, _, _, _ = struct.unpack_from("<IIIIIIII", data, off)
        if p_type == 1 and filesz and 0x10000000 <= paddr < 0x11000000:
            if load is None or paddr < load:
                load = paddr
    if load is None:
        raise SystemExit(f"{path}: nessun segmento in flash")
    return entry, load


def read_orientation(data: bytes, path: str) -> int:
    """0 orizzontale, 1 verticale. Senza il simbolo lippus_orientation vale 0."""
    elf32(data, path)
    e_shoff = struct.unpack_from("<I", data, 32)[0]
    e_shentsize, e_shnum = struct.unpack_from("<HH", data, 46)
    if e_shoff == 0 or e_shnum == 0 or e_shentsize < 40:
        return 0
    sym_off = sym_size = sym_entsize = str_off = str_size = 0
    found_sym = False
    for index in range(e_shnum):
        off = e_shoff + index * e_shentsize
        if off + 40 > len(data):
            raise SystemExit(f"{path}: sezione ELF fuori dal file")
        _, sh_type, _, _, sh_offset, sh_size, sh_link, _, _, sh_entsize = struct.unpack_from(
            "<IIIIIIIIII", data, off
        )
        if sh_type == 2 and sh_entsize >= 16 and sh_link < e_shnum:
            link = e_shoff + sh_link * e_shentsize
            str_off, str_size = struct.unpack_from("<II", data, link + 16)
            sym_off, sym_size, sym_entsize = sh_offset, sh_size, sh_entsize
            found_sym = True
    if not found_sym:
        return 0
    for index in range(sym_size // sym_entsize):
        off = sym_off + index * sym_entsize
        if off + 16 > len(data):
            break
        st_name, st_value, _, _, _, st_shndx = struct.unpack_from("<IIIBBH", data, off)
        if st_name == 0 or st_shndx == 0 or st_shndx >= e_shnum:
            continue
        name_at = str_off + st_name
        if name_at >= str_off + str_size or name_at >= len(data):
            continue
        end = data.find(b"\0", name_at, min(len(data), str_off + str_size))
        if data[name_at:end] != b"lippus_orientation":
            continue
        sec = e_shoff + st_shndx * e_shentsize
        sh_type, _, sh_addr, sh_offset = struct.unpack_from("<IIII", data, sec + 4)
        if sh_type == 8:
            raise SystemExit(f"{path}: lippus_orientation deve essere const")
        file_off = sh_offset + (st_value - sh_addr)
        if file_off < 0 or file_off + 4 > len(data):
            raise SystemExit(f"{path}: lippus_orientation illeggibile")
        value = struct.unpack_from("<I", data, file_off)[0]
        if value > 1:
            raise SystemExit(f"{path}: lippus_orientation {value} non valido")
        return value
    return 0


def pack(elf: str, blob: str, name: str, out: str, expect_load: int) -> None:
    elf_data = open(elf, "rb").read()
    entry, load = read_elf_load(elf_data, elf)
    flags = LBG_FLAG_PORTRAIT if read_orientation(elf_data, elf) else 0
    payload = open(blob, "rb").read()
    if load != expect_load:
        raise SystemExit(
            f"indirizzo di carico 0x{load:08X}, atteso 0x{expect_load:08X}"
        )
    if entry & 1 == 0:
        raise SystemExit("l'entry non e' Thumb")
    if not (load <= (entry & ~1) < load + len(payload)):
        raise SystemExit("entry fuori dal binario")
    raw = name.encode("ascii", "replace")[:31]
    raw = raw + b"\0" * (32 - len(raw))
    header = struct.pack(
        "<4sHHIIII32sII",
        EXPECT_MAGIC,
        HEADER_BYTES,
        API_VERSION,
        len(payload),
        load,
        entry,
        flags,
        raw,
        crc32(payload),
        0,
    )
    if len(header) != HEADER_BYTES:
        raise SystemExit(f"header da {len(header)} byte, attesi {HEADER_BYTES}")
    with open(out, "wb") as handle:
        handle.write(header)
        handle.write(payload)


def elf_with_orientation(value: int) -> bytes:
    text = b"\x00\xbf\x00\xbf" + struct.pack("<I", value)
    strtab = b"\0lippus_orientation\0"
    names = [b"", b".text", b".strtab", b".shstrtab", b".symtab"]
    shstr = bytearray(b"\0")
    sh_name = []
    for name in names:
        sh_name.append(len(shstr))
        if name:
            shstr += name + b"\0"
    symtab = struct.pack("<IIIBBH", 0, 0, 0, 0, 0, 0) + struct.pack(
        "<IIIBBH", 1, 0x10040004, 4, 0x11, 0, 1
    )
    pieces = [b"\0" * 52, b"\0" * 32, text, strtab, bytes(shstr), symtab]
    offsets = []
    cursor = 0
    blob = bytearray()
    for piece in pieces:
        offsets.append(cursor)
        blob += piece
        cursor += len(piece)
    ehdr, phdr, text_off, str_off, shstr_off, sym_off = offsets
    shoff = len(blob)
    sections = [
        (0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        (sh_name[1], 1, 6, 0x10040000, text_off, len(text), 0, 0, 4, 0),
        (sh_name[2], 3, 0, 0, str_off, len(strtab), 0, 0, 1, 0),
        (sh_name[3], 3, 0, 0, shstr_off, len(shstr), 0, 0, 1, 0),
        (sh_name[4], 2, 0, 0, sym_off, len(symtab), 2, 1, 4, 16),
    ]
    for section in sections:
        blob += struct.pack("<IIIIIIIIII", *section)
    struct.pack_into("<4sBBBB", blob, ehdr, b"\x7fELF", 1, 1, 1, 0)
    struct.pack_into("<HHI", blob, 16, 2, 40, 1)
    struct.pack_into("<I", blob, 24, 0x10040001)
    struct.pack_into("<I", blob, 28, phdr)
    struct.pack_into("<I", blob, 32, shoff)
    struct.pack_into("<HHHHHH", blob, 40, 52, 32, 1, 40, 5, 3)
    struct.pack_into("<IIIIIIII", blob, phdr, 1, text_off, 0x10040000, 0x10040000, len(text), len(text), 5, 4)
    return bytes(blob)


def self_test() -> None:
    vector = crc32(b"123456789")
    if vector != 0xCBF43926:
        raise SystemExit(f"crc32 inatteso {vector:08X}")
    elf = bytearray(52 + 32)
    elf[0:4] = b"\x7fELF"
    elf[4] = 1
    elf[5] = 1
    elf[6] = 1
    struct.pack_into("<HHI", elf, 16, 2, 40, 1)
    struct.pack_into("<I", elf, 24, 0x10040001)
    struct.pack_into("<I", elf, 28, 52)
    struct.pack_into("<HHHHHH", elf, 40, 52, 32, 1, 0, 0, 0)
    struct.pack_into("<IIIIIIII", elf, 52, 1, 0, 0x10040000, 0x10040000, 4, 4, 5, 4)
    payload = b"\x00\xbf\x00\xbf"
    import tempfile, os

    with tempfile.TemporaryDirectory() as tmp:
        elf_path = os.path.join(tmp, "g.elf")
        bin_path = os.path.join(tmp, "g.bin")
        out_path = os.path.join(tmp, "g.lbg")
        open(elf_path, "wb").write(elf)
        open(bin_path, "wb").write(payload)
        pack(elf_path, bin_path, "Prova", out_path, 0x10040000)
        packed = open(out_path, "rb").read()
        portrait_elf = os.path.join(tmp, "p.elf")
        portrait_bin = os.path.join(tmp, "p.bin")
        portrait_out = os.path.join(tmp, "p.lbg")
        portrait = elf_with_orientation(1)
        open(portrait_elf, "wb").write(portrait)
        open(portrait_bin, "wb").write(portrait[84:92])
        pack(portrait_elf, portrait_bin, "Alto", portrait_out, 0x10040000)
        vertical = open(portrait_out, "rb").read()
    if packed[:4] != b"LBG1" or len(packed) != 64 + 4:
        raise SystemExit("pacchetto di prova non valido")
    if packed[64:] != payload or struct.unpack_from("<I", packed, 20)[0] != 0:
        raise SystemExit("payload alterato")
    if struct.unpack_from("<I", vertical, 20)[0] != LBG_FLAG_PORTRAIT:
        raise SystemExit("orientamento verticale non scritto")
    print("pack_lbg self-test ok")


def main() -> None:
    if len(sys.argv) == 2 and sys.argv[1] == "--self-test":
        self_test()
        return
    parser = argparse.ArgumentParser(description="Crea un file .lbg")
    parser.add_argument("--elf", required=True)
    parser.add_argument("--bin", required=True)
    parser.add_argument("--name", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--expect-load", default="0x10040000")
    args = parser.parse_args()
    pack(args.elf, args.bin, args.name, args.out, int(args.expect_load, 0))


if __name__ == "__main__":
    main()
