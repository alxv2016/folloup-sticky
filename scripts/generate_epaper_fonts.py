#!/usr/bin/env python3

from __future__ import annotations

import math
import platform
import re
import sys
from ctypes import (
    CDLL,
    POINTER,
    Structure,
    addressof,
    c_bool,
    c_char_p,
    c_double,
    c_int,
    c_long,
    c_size_t,
    c_uint16,
    c_uint32,
    c_uint8,
    c_void_p,
)
from dataclasses import dataclass
from pathlib import Path


if platform.system() != "Darwin":
    raise SystemExit("font generation is only supported on macOS")


class CGPoint(Structure):
    _fields_ = [("x", c_double), ("y", c_double)]


class CGSize(Structure):
    _fields_ = [("width", c_double), ("height", c_double)]


class CGRect(Structure):
    _fields_ = [("origin", CGPoint), ("size", CGSize)]


class CGAffineTransform(Structure):
    _fields_ = [
        ("a", c_double),
        ("b", c_double),
        ("c", c_double),
        ("d", c_double),
        ("tx", c_double),
        ("ty", c_double),
    ]


@dataclass(frozen=True)
class FontSpec:
    path: str
    symbol: str
    size: float


@dataclass(frozen=True)
class GlyphRecord:
    width: int
    height: int
    bearing_x: int
    bearing_y: int
    advance: int
    bitmap: list[int]


class FontGenError(RuntimeError):
    pass


CORE_GRAPHICS = CDLL(
    "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics"
)
CORE_TEXT = CDLL("/System/Library/Frameworks/CoreText.framework/CoreText")

kCTFontOrientationHorizontal = 0
kCGInterpolationNone = 1
kCGImageAlphaNone = 0
kAsciiFirst = 32
kAsciiLast = 126
kIdentityTransform = CGAffineTransform(1.0, 0.0, 0.0, 1.0, 0.0, 0.0)

CORE_GRAPHICS.CGDataProviderCreateWithFilename.argtypes = [c_char_p]
CORE_GRAPHICS.CGDataProviderCreateWithFilename.restype = c_void_p
CORE_GRAPHICS.CGFontCreateWithDataProvider.argtypes = [c_void_p]
CORE_GRAPHICS.CGFontCreateWithDataProvider.restype = c_void_p
CORE_GRAPHICS.CGColorSpaceCreateDeviceGray.argtypes = []
CORE_GRAPHICS.CGColorSpaceCreateDeviceGray.restype = c_void_p
CORE_GRAPHICS.CGBitmapContextCreate.argtypes = [
    c_void_p,
    c_size_t,
    c_size_t,
    c_size_t,
    c_size_t,
    c_void_p,
    c_uint32,
]
CORE_GRAPHICS.CGBitmapContextCreate.restype = c_void_p
CORE_GRAPHICS.CGContextSetGrayFillColor.argtypes = [c_void_p, c_double, c_double]
CORE_GRAPHICS.CGContextFillRect.argtypes = [c_void_p, CGRect]
CORE_GRAPHICS.CGContextSetShouldAntialias.argtypes = [c_void_p, c_bool]
CORE_GRAPHICS.CGContextSetAllowsAntialiasing.argtypes = [c_void_p, c_bool]
CORE_GRAPHICS.CGContextSetInterpolationQuality.argtypes = [c_void_p, c_int]
CORE_GRAPHICS.CGContextSetTextMatrix.argtypes = [c_void_p, CGAffineTransform]

CORE_TEXT.CTFontCreateWithGraphicsFont.argtypes = [
    c_void_p,
    c_double,
    c_void_p,
    c_void_p,
]
CORE_TEXT.CTFontCreateWithGraphicsFont.restype = c_void_p
CORE_TEXT.CTFontGetGlyphsForCharacters.argtypes = [
    c_void_p,
    POINTER(c_uint16),
    POINTER(c_uint16),
    c_long,
]
CORE_TEXT.CTFontGetGlyphsForCharacters.restype = c_bool
CORE_TEXT.CTFontGetAdvancesForGlyphs.argtypes = [
    c_void_p,
    c_uint32,
    POINTER(c_uint16),
    POINTER(CGSize),
    c_long,
]
CORE_TEXT.CTFontGetBoundingRectsForGlyphs.argtypes = [
    c_void_p,
    c_uint32,
    POINTER(c_uint16),
    POINTER(CGRect),
    c_long,
]
CORE_TEXT.CTFontGetBoundingRectsForGlyphs.restype = CGRect
CORE_TEXT.CTFontGetAscent.argtypes = [c_void_p]
CORE_TEXT.CTFontGetAscent.restype = c_double
CORE_TEXT.CTFontGetDescent.argtypes = [c_void_p]
CORE_TEXT.CTFontGetDescent.restype = c_double
CORE_TEXT.CTFontGetLeading.argtypes = [c_void_p]
CORE_TEXT.CTFontGetLeading.restype = c_double
CORE_TEXT.CTFontDrawGlyphs.argtypes = [
    c_void_p,
    POINTER(c_uint16),
    POINTER(CGPoint),
    c_long,
    c_void_p,
]


def parse_args(argv: list[str]) -> tuple[Path, list[FontSpec]]:
    if len(argv) < 4 or argv[1] != "--output":
        raise FontGenError(
            "usage: generate_epaper_fonts.py --output <file> <font-path:symbol:size>..."
        )

    output_path = Path(argv[2])
    specs: list[FontSpec] = []
    for raw in argv[3:]:
        parts = raw.split(":", 2)
        if len(parts) != 3:
            raise FontGenError(f"invalid font spec: {raw}")
        try:
            size = float(parts[2])
        except ValueError as exc:
            raise FontGenError(f"invalid font size in spec: {raw}") from exc
        specs.append(FontSpec(path=parts[0], symbol=parts[1], size=size))

    return output_path, specs


def load_font(spec: FontSpec) -> c_void_p:
    provider = CORE_GRAPHICS.CGDataProviderCreateWithFilename(
        spec.path.encode("utf-8")
    )
    if not provider:
        raise FontGenError(f"unable to load font: {spec.path}")

    cg_font = CORE_GRAPHICS.CGFontCreateWithDataProvider(provider)
    if not cg_font:
        raise FontGenError(f"unable to create CGFont: {spec.path}")

    ct_font = CORE_TEXT.CTFontCreateWithGraphicsFont(cg_font, spec.size, None, None)
    if not ct_font:
        raise FontGenError(f"unable to create CTFont: {spec.path}")

    return ct_font


def resolve_glyph(font: c_void_p, character: int) -> int:
    code_unit = c_uint16(character)
    glyph = c_uint16()
    ok = CORE_TEXT.CTFontGetGlyphsForCharacters(font, code_unit, glyph, 1)
    if ok:
        return glyph.value
    if character != ord("?"):
        return resolve_glyph(font, ord("?"))
    raise FontGenError("font is missing fallback glyph '?'")


def glyph_advance(font: c_void_p, glyph_value: int) -> int:
    glyph = c_uint16(glyph_value)
    advance = CGSize()
    CORE_TEXT.CTFontGetAdvancesForGlyphs(
        font, kCTFontOrientationHorizontal, glyph, advance, 1
    )
    return max(1, math.ceil(advance.width))


def glyph_bounds(font: c_void_p, glyph_value: int) -> tuple[int, int, int, int]:
    glyph = c_uint16(glyph_value)
    bounds = CGRect()
    CORE_TEXT.CTFontGetBoundingRectsForGlyphs(
        font, kCTFontOrientationHorizontal, glyph, bounds, 1
    )
    min_x = math.floor(bounds.origin.x)
    max_x = math.ceil(bounds.origin.x + bounds.size.width)
    min_y = math.floor(bounds.origin.y)
    max_y = math.ceil(bounds.origin.y + bounds.size.height)
    return min_x, max_x, min_y, max_y


def create_bitmap_context(width: int, height: int) -> tuple[c_void_p, object]:
    bytes_per_row = width
    buffer_size = width * height
    pixels = (c_uint8 * buffer_size)(*([0xFF] * buffer_size))
    colorspace = CORE_GRAPHICS.CGColorSpaceCreateDeviceGray()
    context = CORE_GRAPHICS.CGBitmapContextCreate(
        addressof(pixels),
        width,
        height,
        8,
        bytes_per_row,
        colorspace,
        kCGImageAlphaNone,
    )
    if not context:
        raise FontGenError("unable to create bitmap context")

    CORE_GRAPHICS.CGContextSetGrayFillColor(context, 1.0, 1.0)
    CORE_GRAPHICS.CGContextFillRect(
        context, CGRect(CGPoint(0.0, 0.0), CGSize(float(width), float(height)))
    )
    CORE_GRAPHICS.CGContextSetShouldAntialias(context, False)
    CORE_GRAPHICS.CGContextSetAllowsAntialiasing(context, False)
    CORE_GRAPHICS.CGContextSetInterpolationQuality(context, kCGInterpolationNone)
    CORE_GRAPHICS.CGContextSetGrayFillColor(context, 0.0, 1.0)
    CORE_GRAPHICS.CGContextSetTextMatrix(context, kIdentityTransform)
    return context, pixels


def pack_bitmap(
    pixels: object, width: int, height: int, bytes_per_row: int
) -> list[int]:
    packed: list[int] = []
    current_byte = 0
    bit_index = 0

    for row in range(height):
        for col in range(width):
            sample = pixels[row * bytes_per_row + col]
            if sample < 128:
                current_byte |= 0x80 >> bit_index
            bit_index += 1
            if bit_index == 8:
                packed.append(current_byte)
                current_byte = 0
                bit_index = 0

    if bit_index:
        packed.append(current_byte)
    return packed


def render_glyph(font: c_void_p, character: int) -> GlyphRecord:
    glyph_value = resolve_glyph(font, character)
    advance = glyph_advance(font, glyph_value)
    min_x, max_x, min_y, max_y = glyph_bounds(font, glyph_value)

    width = max(0, max_x - min_x)
    height = max(0, max_y - min_y)
    if width == 0 or height == 0:
        return GlyphRecord(
            width=0,
            height=0,
            bearing_x=min_x,
            bearing_y=max_y,
            advance=advance,
            bitmap=[],
        )

    context, pixels = create_bitmap_context(width, height)
    glyph = c_uint16(glyph_value)
    position = CGPoint(float(-min_x), float(-min_y))
    CORE_TEXT.CTFontDrawGlyphs(font, glyph, position, 1, context)

    bitmap = pack_bitmap(pixels, width, height, width)
    return GlyphRecord(
        width=width,
        height=height,
        bearing_x=min_x,
        bearing_y=max_y,
        advance=advance,
        bitmap=bitmap,
    )


def emit_byte_array(values: list[int], indent: str) -> str:
    if not values:
        return ""

    lines: list[str] = []
    line = indent
    for index, value in enumerate(values):
        entry = f"0x{value:02X}"
        if index == 0:
            line += entry
        elif len(line) + len(entry) + 2 > 100:
            lines.append(line + ",")
            line = indent + entry
        else:
            line += ", " + entry
    lines.append(line)
    return "\n".join(lines)


def generate_source(specs: list[FontSpec]) -> str:
    out = [
        '#include "generated_epaper_fonts.h"',
        "",
        "namespace epaper_fonts {",
        "",
    ]

    for spec in specs:
        font = load_font(spec)
        ascent = max(1, math.ceil(CORE_TEXT.CTFontGetAscent(font)))
        descent = max(0, math.ceil(CORE_TEXT.CTFontGetDescent(font)))
        leading = max(0, math.ceil(CORE_TEXT.CTFontGetLeading(font)))
        line_height = max(1, ascent + descent + leading)

        all_bitmap_bytes: list[int] = []
        glyph_lines: list[str] = []
        for code in range(kAsciiFirst, kAsciiLast + 1):
            glyph = render_glyph(font, code)
            offset = len(all_bitmap_bytes)
            all_bitmap_bytes.extend(glyph.bitmap)
            glyph_lines.append(
                "    { %5u, %4u, %3u, %3u, %4d, %4d, %3u },"
                % (
                    offset,
                    len(glyph.bitmap),
                    glyph.width,
                    glyph.height,
                    glyph.bearing_x,
                    glyph.bearing_y,
                    glyph.advance,
                )
            )

        out.extend(
            [
                "namespace {",
                f"const epaper_font::GlyphBitmap {spec.symbol}_glyphs[] = {{",
                *glyph_lines,
                "};",
                "",
                f"const uint8_t {spec.symbol}_bitmaps[] = {{",
            ]
        )
        if all_bitmap_bytes:
            out.append(emit_byte_array(all_bitmap_bytes, indent="    "))
        out.extend(
            [
                "};",
                "}  // namespace",
                "",
                f"const epaper_font::BitmapFont {spec.symbol} = {{",
                f'    "{spec.symbol}",',
                f"    {kAsciiFirst},",
                f"    {kAsciiLast},",
                f"    {line_height},",
                f"    {ascent},",
                f"    {spec.symbol}_glyphs,",
                f"    {spec.symbol}_bitmaps,",
                "};",
                "",
            ]
        )

    out.append("}  // namespace epaper_fonts")
    out.append("")
    return "\n".join(out)


def main(argv: list[str]) -> int:
    try:
        output_path, specs = parse_args(argv)
        source = generate_source(specs)
        output_path.write_text(source, encoding="utf-8")
        return 0
    except FontGenError as exc:
        sys.stderr.write(f"font generation failed: {exc}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
