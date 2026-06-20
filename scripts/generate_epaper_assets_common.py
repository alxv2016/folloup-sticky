#!/usr/bin/env python3

from __future__ import annotations

import struct
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class AssetSpec:
    path: Path
    symbol: str


@dataclass(frozen=True)
class BitmapAsset:
    width: int
    height: int
    stride_bytes: int
    data: list[int]


class AssetGenError(RuntimeError):
    pass


def parse_asset_spec(raw: str) -> AssetSpec:
    parts = raw.split(":")
    if len(parts) != 2:
        raise AssetGenError(f"invalid asset spec: {raw}")
    path = Path(parts[0])
    if not path.is_file():
        raise AssetGenError(f"asset file not found: {path}")
    return AssetSpec(path=path, symbol=parts[1])


def convert_png_to_bmp(
    asset_path: Path,
    temp_dir: str,
    *,
    fixed_size: int | None = None,
) -> Path:
    bmp_path = Path(temp_dir) / f"{asset_path.stem}.bmp"
    command = ["sips"]
    if fixed_size is not None:
        command.extend(["-z", str(fixed_size), str(fixed_size)])
    command.extend(
        [
            "-s",
            "format",
            "bmp",
            str(asset_path),
            "--out",
            str(bmp_path),
        ]
    )
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        raise AssetGenError(
            f"failed to convert {asset_path} with sips:\n{result.stderr.strip()}"
        )
    if not bmp_path.is_file():
        raise AssetGenError(f"sips did not produce output for {asset_path}")
    return bmp_path


def parse_bmp_pixels(bmp_path: Path) -> tuple[int, int, list[int]]:
    data = bmp_path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise AssetGenError(f"unsupported BMP file: {bmp_path}")

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    if dib_size < 40:
        raise AssetGenError(f"unsupported BMP DIB header in {bmp_path}")

    width = struct.unpack_from("<i", data, 18)[0]
    height = struct.unpack_from("<i", data, 22)[0]
    bits_per_pixel = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]

    if width <= 0 or height == 0:
        raise AssetGenError(f"invalid BMP dimensions in {bmp_path}")
    if bits_per_pixel not in (24, 32):
        raise AssetGenError(
            f"unsupported BMP bit depth {bits_per_pixel} in {bmp_path}"
        )
    if compression not in (0, 3):
        raise AssetGenError(
            f"unsupported BMP compression {compression} in {bmp_path}"
        )

    abs_height = abs(height)
    top_down = height < 0
    bytes_per_pixel = bits_per_pixel // 8
    row_stride = ((width * bits_per_pixel + 31) // 32) * 4
    pixels = [0x00] * (width * abs_height)

    for row in range(abs_height):
        source_row = row if top_down else abs_height - 1 - row
        row_start = pixel_offset + source_row * row_stride
        for col in range(width):
            pixel_start = row_start + col * bytes_per_pixel
            blue = data[pixel_start]
            green = data[pixel_start + 1]
            red = data[pixel_start + 2]
            alpha = data[pixel_start + 3] if bytes_per_pixel == 4 else 0xFF
            luminance = (red * 30 + green * 59 + blue * 11) // 100
            filled = alpha >= 16 and luminance < 245
            pixels[row * width + col] = 1 if filled else 0

    return width, abs_height, pixels


def pack_bitmap(width: int, height: int, pixels: list[int]) -> BitmapAsset:
    stride_bytes = (width + 7) // 8
    packed: list[int] = []
    for row in range(height):
        row_start = row * width
        for byte_index in range(stride_bytes):
            value = 0
            for bit in range(8):
                col = byte_index * 8 + bit
                value <<= 1
                if col < width and pixels[row_start + col]:
                    value |= 0x01
            packed.append(value)
    return BitmapAsset(width=width, height=height, stride_bytes=stride_bytes, data=packed)


def emit_byte_array(values: list[int], indent: str) -> list[str]:
    if not values:
        return [f"{indent}0x00,"]
    lines: list[str] = []
    chunk_size = 12
    for start in range(0, len(values), chunk_size):
        chunk = values[start : start + chunk_size]
        lines.append(indent + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    return lines


def write_header(output_path: Path, specs: list[AssetSpec], *, guard: str, namespace: str) -> None:
    lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        '#include "asset_types.h"',
        "",
        f"namespace {namespace} {{",
        "",
    ]
    for spec in specs:
        lines.append(f"extern const EmbeddedImageAsset {spec.symbol};")
    lines.extend(
        [
            "",
            f"}}  // namespace {namespace}",
            "",
            f"#endif  // {guard}",
            "",
        ]
    )
    output_path.write_text("\n".join(lines), encoding="utf-8")


def write_source(
    output_path: Path,
    header_name: str,
    specs: list[AssetSpec],
    bitmaps: list[BitmapAsset],
    *,
    namespace: str,
) -> None:
    lines = [
        f'#include "{header_name}"',
        "",
        f"namespace {namespace} {{",
        "",
    ]

    for spec, bitmap in zip(specs, bitmaps):
        lines.append(f"const uint8_t {spec.symbol}_data[] = {{")
        lines.extend(emit_byte_array(bitmap.data, "    "))
        lines.append("};")
        lines.append("")
        lines.append(f"const EmbeddedImageAsset {spec.symbol} = {{")
        lines.append(f"    {spec.symbol}_data,")
        lines.append(f"    {bitmap.width},")
        lines.append(f"    {bitmap.height},")
        lines.append(f"    {bitmap.stride_bytes},")
        lines.append("    ImageFormat::kMono1,")
        lines.append("};")
        lines.append("")

    lines.extend([f"}}  // namespace {namespace}", ""])
    output_path.write_text("\n".join(lines), encoding="utf-8")


def build_bitmaps(specs: list[AssetSpec], *, fixed_size: int | None) -> list[BitmapAsset]:
    bitmaps: list[BitmapAsset] = []
    with tempfile.TemporaryDirectory() as temp_dir:
        for spec in specs:
            bmp_path = convert_png_to_bmp(spec.path, temp_dir, fixed_size=fixed_size)
            width, height, pixels = parse_bmp_pixels(bmp_path)
            bitmaps.append(pack_bitmap(width, height, pixels))
    return bitmaps
