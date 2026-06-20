#!/usr/bin/env python3

from __future__ import annotations

import argparse
from pathlib import Path

from generate_epaper_assets_common import (
    AssetGenError,
    build_bitmaps,
    parse_asset_spec,
    write_header,
    write_source,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate fixed-size 36x36 embedded monochrome e-paper icons from PNG assets."
    )
    parser.add_argument("--output-header", required=True)
    parser.add_argument("--output-source", required=True)
    parser.add_argument("icons", nargs="+", help="icon specs in path:symbol form")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    specs = [parse_asset_spec(raw) for raw in args.icons]
    bitmaps = build_bitmaps(specs, fixed_size=36)

    header_path = Path(args.output_header)
    source_path = Path(args.output_source)
    write_header(
        header_path,
        specs,
        guard="GENERATED_EPAPER_ICONS_H",
        namespace="epaper_icons",
    )
    write_source(
        source_path,
        header_path.name,
        specs,
        bitmaps,
        namespace="epaper_icons",
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssetGenError as exc:
        raise SystemExit(str(exc))
