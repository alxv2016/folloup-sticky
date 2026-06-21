# Asset Generation

The e-paper UI asset generation pipeline is staged in this repo so the app UI
can use generated monochrome image and bitmap-font assets.

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

## Embedded Bitmap Fonts

Bitmap font generation is now compiled through `components/epaper_ui/`:

- `components/epaper_ui/generated_epaper_fonts.cpp`
- `components/epaper_ui/include/epaper_ui/generated_epaper_fonts.h`

The current renderer uses the same Inter font sizes and weights expected by the
ported `followup` design-token roles.

Regenerate the font source with `scripts/generate_epaper_fonts.py`, then keep
the small public header in sync if new symbols are added.

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

Example:

```bash
python3 scripts/generate_epaper_fonts.py \
  --output components/epaper_ui/generated_epaper_fonts.cpp \
  fonts/Inter_18pt-SemiBold.ttf:kInter22SemiBold:22 \
  fonts/Inter_18pt-Bold.ttf:kInter22Bold:22 \
  fonts/Inter_18pt-Black.ttf:kInter22Black:22 \
  fonts/Inter_18pt-SemiBold.ttf:kInter26SemiBold:26 \
  fonts/Inter_18pt-Bold.ttf:kInter26Bold:26 \
  fonts/Inter_18pt-Black.ttf:kInter26Black:26 \
  fonts/Inter_24pt-SemiBold.ttf:kInter32SemiBold:32 \
  fonts/Inter_24pt-Bold.ttf:kInter32Bold:32 \
  fonts/Inter_24pt-Black.ttf:kInter32Black:32 \
  fonts/Inter_24pt-SemiBold.ttf:kInter38SemiBold:38 \
  fonts/Inter_24pt-Bold.ttf:kInter38Bold:38 \
  fonts/Inter_24pt-Black.ttf:kInter38Black:38 \
  fonts/Inter_24pt-SemiBold.ttf:kInter46SemiBold:46 \
  fonts/Inter_24pt-Bold.ttf:kInter46Bold:46 \
  fonts/Inter_24pt-Black.ttf:kInter46Black:46 \
  fonts/Inter_28pt-SemiBold.ttf:kInter55SemiBold:55 \
  fonts/Inter_28pt-Bold.ttf:kInter55Bold:55 \
  fonts/Inter_28pt-Black.ttf:kInter55Black:55 \
  fonts/Inter_28pt-SemiBold.ttf:kInter165SemiBold:165 \
  fonts/Inter_28pt-Bold.ttf:kInter165Bold:165 \
  fonts/Inter_28pt-Black.ttf:kInter165Black:165
```

## Policy

Treat the PNG and TTF files as canonical source assets. Generated C++ files
should be reproducible from those inputs and the scripts, and should not be
edited by hand.
