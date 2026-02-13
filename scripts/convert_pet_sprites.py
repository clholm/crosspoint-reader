#!/usr/bin/env python3
"""Convert pet sprites between 64x64 1-bit PNG and C header formats.

Requires: pip install Pillow

Usage:
    # Extract existing headers to editable PNGs (one-time bootstrap)
    python scripts/convert_pet_sprites.py h2png src/images/pet/*.h

    # Convert edited PNGs back to headers
    python scripts/convert_pet_sprites.py png2h src/images/pet/PetHappy.png

    # Sync all: regenerate headers for any PNG newer than its header
    python scripts/convert_pet_sprites.py sync [--dir src/images/pet]
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

try:
    from PIL import Image
except ImportError:
    print("Pillow is required: pip install Pillow", file=sys.stderr)
    sys.exit(1)

SPRITE_SIZE = 64
BYTES_PER_ROW = SPRITE_SIZE // 8
TOTAL_BYTES = BYTES_PER_ROW * SPRITE_SIZE  # 512
DEFAULT_PET_DIR = pathlib.Path("src/images/pet")


def png_to_bytes(png_path: pathlib.Path) -> list[int]:
    """Load a PNG and convert to 512 bytes of 1-bit MSB-first bitmap data.

    Handles any input size/mode: resizes to 64x64 (preserving aspect ratio,
    centered on white), composites transparency onto white, and thresholds
    at 128 to produce 1-bit output. Bit encoding: 1 = white, 0 = black.
    """
    img = Image.open(png_path)

    # Composite RGBA transparency onto white background
    if img.mode == "RGBA":
        bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
        bg.paste(img, mask=img.split()[3])
        img = bg

    img = img.convert("L")

    # Resize to 64x64 if needed, preserving aspect ratio
    if img.size != (SPRITE_SIZE, SPRITE_SIZE):
        img.thumbnail((SPRITE_SIZE, SPRITE_SIZE), Image.LANCZOS)
        canvas = Image.new("L", (SPRITE_SIZE, SPRITE_SIZE), 255)
        offset = ((SPRITE_SIZE - img.width) // 2, (SPRITE_SIZE - img.height) // 2)
        canvas.paste(img, offset)
        img = canvas

    # Pack pixels into bytes: MSB-first, 1 = white, 0 = black
    pixels = img.load()
    data = []
    for y in range(SPRITE_SIZE):
        for byte_idx in range(BYTES_PER_ROW):
            byte_val = 0
            for bit in range(8):
                x = byte_idx * 8 + bit
                if pixels[x, y] >= 128:  # white
                    byte_val |= 1 << (7 - bit)
            data.append(byte_val)

    return data


def bytes_to_png(data: list[int], png_path: pathlib.Path) -> None:
    """Convert 512 bytes of 1-bit MSB-first bitmap data to a 64x64 PNG."""
    if len(data) != TOTAL_BYTES:
        raise ValueError(f"Expected {TOTAL_BYTES} bytes, got {len(data)}")

    img = Image.new("L", (SPRITE_SIZE, SPRITE_SIZE), 255)
    pixels = img.load()

    for y in range(SPRITE_SIZE):
        for byte_idx in range(BYTES_PER_ROW):
            byte_val = data[y * BYTES_PER_ROW + byte_idx]
            for bit in range(8):
                x = byte_idx * 8 + bit
                pixel_set = (byte_val >> (7 - bit)) & 1
                pixels[x, y] = 255 if pixel_set else 0

    img.save(png_path)


def parse_header(header_path: pathlib.Path) -> tuple[str, list[int]]:
    """Parse a C header file, returning (array_name, byte_data)."""
    text = header_path.read_text()

    match = re.search(r"uint8_t\s+(\w+)\s*\[\]", text)
    if not match:
        raise ValueError(f"Could not find uint8_t array in {header_path}")
    array_name = match.group(1)

    hex_values = re.findall(r"0[xX][0-9A-Fa-f]{2}", text)
    data = [int(v, 16) for v in hex_values]

    if len(data) != TOTAL_BYTES:
        raise ValueError(
            f"Expected {TOTAL_BYTES} bytes in {header_path}, got {len(data)}"
        )

    return array_name, data


def format_header(array_name: str, data: list[int]) -> str:
    """Format byte data into a C header matching the existing pet sprite format."""
    lines = [
        "#pragma once",
        "#include <cstdint>",
        "",
        f"static const uint8_t {array_name}[] = {{",
    ]

    for row in range(SPRITE_SIZE):
        row_bytes = data[row * BYTES_PER_ROW : (row + 1) * BYTES_PER_ROW]
        hex_str = ", ".join(f"0x{b:02X}" for b in row_bytes)
        if row < SPRITE_SIZE - 1:
            lines.append(f"    {hex_str},")
        else:
            lines.append(f"    {hex_str}")

    lines.append("};")
    lines.append("")  # trailing newline

    return "\n".join(lines)


def sync_directory(pet_dir: pathlib.Path) -> int:
    """Regenerate .h for each .png that is newer than its corresponding .h.

    Returns the number of files converted.
    """
    converted = 0
    for png_path in sorted(pet_dir.glob("*.png")):
        header_path = png_path.with_suffix(".h")
        array_name = png_path.stem

        if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", array_name):
            print(f"  skip: {png_path.name} (not a valid C identifier)", file=sys.stderr)
            continue

        if header_path.exists():
            if png_path.stat().st_mtime <= header_path.stat().st_mtime:
                continue

        data = png_to_bytes(png_path)
        header_path.write_text(format_header(array_name, data))
        print(f"  {png_path.name} -> {header_path.name}")
        converted += 1

    return converted


def cmd_png2h(args: argparse.Namespace) -> None:
    for path in args.files:
        path = pathlib.Path(path)
        if not path.exists():
            print(f"File not found: {path}", file=sys.stderr)
            sys.exit(1)
        array_name = path.stem
        if not re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", array_name):
            print(f"Invalid C identifier: {array_name}", file=sys.stderr)
            sys.exit(1)

        header_path = path.with_suffix(".h")
        data = png_to_bytes(path)
        header_path.write_text(format_header(array_name, data))
        print(f"  {path.name} -> {header_path.name}")


def cmd_h2png(args: argparse.Namespace) -> None:
    for path in args.files:
        path = pathlib.Path(path)
        if not path.exists():
            print(f"File not found: {path}", file=sys.stderr)
            sys.exit(1)

        array_name, data = parse_header(path)
        png_path = path.with_suffix(".png")
        bytes_to_png(data, png_path)
        print(f"  {path.name} -> {png_path.name}")


def cmd_sync(args: argparse.Namespace) -> None:
    pet_dir = pathlib.Path(args.dir)
    if not pet_dir.is_dir():
        print(f"Directory not found: {pet_dir}", file=sys.stderr)
        sys.exit(1)

    converted = sync_directory(pet_dir)
    if converted == 0:
        print("  All headers up to date.")
    else:
        print(f"  Converted {converted} file(s).")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert pet sprites between 64x64 1-bit PNG and C header formats."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    p_png2h = subparsers.add_parser("png2h", help="Convert PNG(s) to C header(s)")
    p_png2h.add_argument("files", nargs="+", type=pathlib.Path)
    p_png2h.set_defaults(func=cmd_png2h)

    p_h2png = subparsers.add_parser("h2png", help="Convert C header(s) to PNG(s)")
    p_h2png.add_argument("files", nargs="+", type=pathlib.Path)
    p_h2png.set_defaults(func=cmd_h2png)

    p_sync = subparsers.add_parser(
        "sync", help="Sync PNGs to headers (skip up-to-date)"
    )
    p_sync.add_argument("--dir", type=pathlib.Path, default=DEFAULT_PET_DIR)
    p_sync.set_defaults(func=cmd_sync)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
