#!/usr/bin/env python3
"""Il binario del firmware deve restare sotto lo slot dei giochi."""

import sys


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("uso: check_os_size.py <firmware.bin> <limite>")
    path, limit = sys.argv[1], int(sys.argv[2], 0)
    size = len(open(path, "rb").read())
    if size >= limit:
        raise SystemExit(
            f"{path} occupa {size} byte, il limite prima dei giochi e' {limit}"
        )
    print(f"firmware {size} byte, slot giochi libero da 0x{limit:X}")


if __name__ == "__main__":
    main()
