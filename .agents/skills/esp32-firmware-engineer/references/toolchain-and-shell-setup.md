# ESP-IDF Toolchain and Shell Setup (ESP32/ESP-IDF)

Use this reference before build/flash/monitor operations and when improving developer UX.

## Pre-Build Toolchain Rule

- Before running `build.sh` / `idf.py build`, verify ESP-IDF is actually usable:
  - `idf.py` resolves
  - `idf.py --version` succeeds
- Do not assume the toolchain is installed because a path exists or an older shell once worked.

## Minimum Preflight Checks

- `command -v idf.py`
- `idf.py --version`
- `python3 --version` (if the build wrappers rely on Python and virtual env tooling)
- project wrapper preflight (if provided) succeeds

If any fail:
- source the ESP-IDF environment (`export.sh` or `~/.esp_idf_env`)
- verify `IDF_PATH`/installation path
- fix shell PATH setup before continuing

## Shell UX Helper (Recommended)

If the user shell lacks a convenient ESP-IDF shortcut, add a small snippet to the shell profile (`~/.zshrc`, `~/.bashrc`, etc.).

### zsh snippet (example)

```sh
# ESP-IDF Environment (auto-load)
# source ~/.esp_idf_env  # Uncomment to auto-load on terminal start
alias idf="source \"$HOME/.esp_idf_env\""
export PATH="$PATH:$HOME/go/bin"
export PATH="$HOME/.local/bin:$PATH"
```

Notes:
- `alias idf=...` provides a quick environment load command.
- Keep auto-loading commented by default unless the user wants every shell to source ESP-IDF.
- Ensure `$HOME/.local/bin` is early enough in `PATH` for user-installed tools.

## Agent Behavior

- If build preflight fails, fix shell/toolchain setup before attempting the build.
- After toolchain preflight, run plugin/framework compatibility preflight before building when ESP-ADF/ESP-SR/etc. are used.
- If a shell helper snippet is missing and the user is using zsh/bash, add one (or propose one) to improve repeated workflow UX.
- Avoid duplicating the snippet if equivalent aliases/PATH entries already exist.

## Review Checklist

- Build preflight checks run before build/flash/monitor.
- Plugin/framework compatibility evidence is checked before build for stacks that use ESP-ADF/ESP-SR/etc.
- ESP-IDF env source path is valid on the current machine.
- Shell helper snippet exists (or user intentionally declined).
- No duplicate/conflicting `idf` aliases or PATH entries introduced.
