# ESP32 Display and Graphics Validation (ESP-IDF)

Use this reference for display bring-up, framebuffer formats, flush paths, and graphics correctness on ESP32 projects.

## First Principle: Validate the Display Data Path

Before writing or changing graphics code, confirm:

- Display controller model (for example ST7789, ILI9341, GC9A01, etc.)
- Interface type (SPI, i80/parallel, RGB, MIPI-DSI on supported targets)
- Resolution and orientation
- Pixel format expected by the controller/path (RGB565, BGR565, RGB888, etc.)
- Byte order / endianness / color order
- Window/flush command protocol and region alignment constraints
- DMA/buffer requirements (alignment, internal RAM vs PSRAM support)

If any of these are unknown, stop and ask before changing graphics code.

## Common Failure Modes (Usually Not "Rendering Logic" Bugs)

- Colors swapped (RGB/BGR mismatch)
- Blue/red swapped or tint issues (byte order / endian mismatch)
- Corrupted lines/tearing (buffer stride, DMA alignment, race in flush ownership)
- Partial updates in wrong region (window coordinates or rotation transform mismatch)
- Random corruption under load (buffer lifetime issue, PSRAM/DMA mismatch, cache/coherency assumptions)

## Buffer and Format Rules

- Convert only to the exact format the display path expects.
- Keep a single documented source-of-truth format at the display boundary.
- Validate stride/line pitch assumptions explicitly.
- Do not assume a library's default color order matches your panel/controller config.

## Performance Considerations

- Match bus clock and DMA usage to board wiring and panel stability limits.
- Prefer DMA-capable buffers for large transfers when supported/required.
- Validate whether the display driver path supports PSRAM-backed buffers on the chosen target and IDF version.
- Use partial updates/dirty rectangles when applicable and correct for the UI stack.

## Review Checklist

- Controller/interface/pixel format explicitly identified.
- Color order and byte order are explicit in code/config.
- Flush buffer lifetime is valid through transaction completion.
- DMA/memory placement meets driver requirements.
- Rotation/window math matches panel configuration.
- Performance tuning changes are measured and remain visually correct.
