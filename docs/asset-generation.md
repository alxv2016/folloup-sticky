# Asset Generation

The e-paper UI asset generation pipeline is staged in this repo so the app UI
can use generated monochrome image and bitmap-font assets when the real
interface is built.

## Source Roots

- `assets/icons/`: source PNG icons for fixed-size embedded icon assets.
- `assets/logos/`: source PNG logos that preserve source aspect ratio.
- `fonts/`: source Inter TTF files for packed bitmap fonts.

The current scripts are macOS-oriented because they use `sips` for PNG
conversion and CoreGraphics/CoreText for font rasterization.

## Scripts

- `scripts/generate_epaper_icons.py`
- `scripts/generate_epaper_footer_icons.py`
- `scripts/generate_epaper_logos.py`
- `scripts/generate_epaper_fonts.py`
- `scripts/generate_epaper_assets_common.py`
- `scripts/generate_epaper_project_assets.py`

The generated image assets depend on
`components/project_assets/asset_types.h`, which defines the shared packed
monochrome image type.

## Project Asset Manifest

The scalable path is `assets/epaper_assets.json`. Add new source assets there,
then regenerate the project asset component with:

```bash
python3 scripts/generate_epaper_project_assets.py
```

That command regenerates:

- `components/project_assets/asset_manifest.h`
- `components/project_assets/project_assets.h`
- `components/project_assets/project_assets.cpp`
- `components/project_assets/generated_epaper_icons.h/.cpp`
- `components/project_assets/generated_epaper_footer_icons.h/.cpp`
- `components/project_assets/generated_epaper_logos.h/.cpp`

`components/project_assets/CMakeLists.txt` already compiles all generated image
sources, including the empty icon/footer generated files. Adding future icons
should not require changing CMake.

## Embedded Logo Assets

Logo assets are currently embedded through `components/project_assets/`:

- `EmbeddedLogoId::kAlxvLabsLogo`
- `EmbeddedLogoId::kFollowupLogo`

They are listed in `assets/epaper_assets.json`. Regenerate them with:

```bash
python3 scripts/generate_epaper_project_assets.py
```

UI code should include `project_assets.h` and use
`project_assets::GetLogo(...)` rather than including generated files directly.

## Embedded Icon Assets

All PNG files in `assets/icons/` are currently embedded as fixed `36x36`
monochrome e-paper icon assets through `EmbeddedIconId` and
`project_assets::GetIcon(...)`.

To add a new icon:

1. Add the source PNG to `assets/icons/`.
2. Add an entry to the `icons` array in `assets/epaper_assets.json`.
3. Run `python3 scripts/generate_epaper_project_assets.py`.

Generated font source/header files should be added later when the app UI has a
stable text rendering path.

## Policy

Treat the PNG and TTF files as canonical source assets. Generated C++ files
should be reproducible from those inputs and the scripts, and should not be
edited by hand.
