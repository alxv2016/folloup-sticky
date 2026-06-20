#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path

from generate_epaper_assets_common import (
    AssetGenError,
    AssetSpec,
    build_bitmaps,
    write_header,
    write_source,
)


@dataclass(frozen=True)
class AssetCategory:
    manifest_key: str
    enum_name: str
    getter_name: str
    namespace: str
    header_name: str
    source_name: str
    guard: str
    fixed_size: int | None


CATEGORIES = [
    AssetCategory(
        manifest_key="logos",
        enum_name="EmbeddedLogoId",
        getter_name="GetLogo",
        namespace="epaper_logos",
        header_name="generated_epaper_logos.h",
        source_name="generated_epaper_logos.cpp",
        guard="GENERATED_EPAPER_LOGOS_H",
        fixed_size=None,
    ),
    AssetCategory(
        manifest_key="icons",
        enum_name="EmbeddedIconId",
        getter_name="GetIcon",
        namespace="epaper_icons",
        header_name="generated_epaper_icons.h",
        source_name="generated_epaper_icons.cpp",
        guard="GENERATED_EPAPER_ICONS_H",
        fixed_size=36,
    ),
    AssetCategory(
        manifest_key="footer_icons",
        enum_name="EmbeddedFooterIconId",
        getter_name="GetFooterIcon",
        namespace="epaper_footer_icons",
        header_name="generated_epaper_footer_icons.h",
        source_name="generated_epaper_footer_icons.cpp",
        guard="GENERATED_EPAPER_FOOTER_ICONS_H",
        fixed_size=44,
    ),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate embedded e-paper project assets from a manifest."
    )
    parser.add_argument(
        "--manifest",
        default="assets/epaper_assets.json",
        help="asset manifest JSON path",
    )
    parser.add_argument(
        "--output-dir",
        default="components/project_assets",
        help="directory for generated C++ asset files",
    )
    return parser.parse_args()


def load_manifest(path: Path) -> dict[str, list[AssetSpec]]:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise AssetGenError(f"failed to read manifest {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise AssetGenError(f"failed to parse manifest {path}: {exc}") from exc

    manifest: dict[str, list[AssetSpec]] = {}
    for category in CATEGORIES:
        entries = raw.get(category.manifest_key, [])
        if not isinstance(entries, list):
            raise AssetGenError(f"{category.manifest_key} must be a list")

        specs: list[AssetSpec] = []
        seen_symbols: set[str] = set()
        for entry in entries:
            if not isinstance(entry, dict):
                raise AssetGenError(f"{category.manifest_key} entries must be objects")
            asset_path = entry.get("path")
            symbol = entry.get("symbol")
            if not isinstance(asset_path, str) or not isinstance(symbol, str):
                raise AssetGenError(
                    f"{category.manifest_key} entries need string path and symbol"
                )
            if symbol in seen_symbols:
                raise AssetGenError(f"duplicate asset symbol: {symbol}")
            seen_symbols.add(symbol)

            path_value = Path(asset_path)
            if not path_value.is_file():
                raise AssetGenError(f"asset file not found: {path_value}")
            specs.append(AssetSpec(path=path_value, symbol=symbol))
        manifest[category.manifest_key] = specs
    return manifest


def write_asset_manifest(output_dir: Path, manifest: dict[str, list[AssetSpec]]) -> None:
    lines = [
        "#ifndef PROJECT_ASSET_MANIFEST_H_",
        "#define PROJECT_ASSET_MANIFEST_H_",
        "",
        "#include <cstdint>",
        "",
    ]
    for category in CATEGORIES:
        lines.append(f"enum class {category.enum_name} : uint8_t {{")
        for spec in manifest[category.manifest_key]:
            lines.append(f"    {spec.symbol},")
        lines.append("};")
        lines.append("")
    lines.extend(["#endif  // PROJECT_ASSET_MANIFEST_H_", ""])
    (output_dir / "asset_manifest.h").write_text("\n".join(lines), encoding="utf-8")


def write_project_assets_header(output_dir: Path) -> None:
    lines = [
        "#ifndef PROJECT_ASSETS_H_",
        "#define PROJECT_ASSETS_H_",
        "",
        '#include "asset_manifest.h"',
        '#include "asset_types.h"',
        "",
        "namespace project_assets {",
        "",
    ]
    for category in CATEGORIES:
        lines.append(
            f"const EmbeddedImageAsset* {category.getter_name}({category.enum_name} id);"
        )
    lines.extend(
        [
            "",
            "}  // namespace project_assets",
            "",
            "#endif  // PROJECT_ASSETS_H_",
            "",
        ]
    )
    (output_dir / "project_assets.h").write_text("\n".join(lines), encoding="utf-8")


def write_project_assets_source(
    output_dir: Path, manifest: dict[str, list[AssetSpec]]
) -> None:
    lines = [
        '#include "project_assets.h"',
        "",
    ]
    for category in CATEGORIES:
        lines.append(f'#include "{category.header_name}"')
    lines.extend(["", "namespace project_assets {", ""])

    for category in CATEGORIES:
        lines.append(
            f"const EmbeddedImageAsset* {category.getter_name}({category.enum_name} id)"
        )
        lines.append("{")
        lines.append("    switch (id) {")
        for spec in manifest[category.manifest_key]:
            lines.append(f"        case {category.enum_name}::{spec.symbol}:")
            lines.append(f"            return &{category.namespace}::{spec.symbol};")
        lines.append("        default:")
        lines.append("            return nullptr;")
        lines.append("    }")
        lines.append("}")
        lines.append("")
    lines.extend(["}  // namespace project_assets", ""])
    (output_dir / "project_assets.cpp").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = parse_args()
    manifest = load_manifest(Path(args.manifest))
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    for category in CATEGORIES:
        specs = manifest[category.manifest_key]
        bitmaps = build_bitmaps(specs, fixed_size=category.fixed_size) if specs else []
        write_header(
            output_dir / category.header_name,
            specs,
            guard=category.guard,
            namespace=category.namespace,
        )
        write_source(
            output_dir / category.source_name,
            category.header_name,
            specs,
            bitmaps,
            namespace=category.namespace,
        )

    write_asset_manifest(output_dir, manifest)
    write_project_assets_header(output_dir)
    write_project_assets_source(output_dir, manifest)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssetGenError as exc:
        raise SystemExit(str(exc))
