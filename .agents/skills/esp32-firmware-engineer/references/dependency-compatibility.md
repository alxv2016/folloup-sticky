# ESP-IDF / Plugin Compatibility Evidence (ESP-ADF, ESP-SR, etc.)

Use this reference before build/debug/flash when external ESP frameworks/plugins are in use. Compatibility must be proven with exact-version evidence.

## Core Rule

- Do not continue until you have concrete evidence that every plugin/framework is compatible with the exact ESP-IDF version and with each other (when they are used together).
- "Concrete evidence" means exact versions + a verifiable source (matrix, manifest, release note, pinned compatibility file, or tested upstream bundle).

## Why This Matters

- ESP-IDF, ESP-ADF, and ESP-SR often have version constraints that are not interchangeable.
- Individual compatibility with ESP-IDF is not enough when multiple frameworks are combined.
- A stack can pass one check (`ESP-SR` says `idf >= 5.0`) and still fail in practice due to an `ESP-ADF` matrix gap or cross-stack mismatch.

## Evidence Sources (Preferred Order)

## 1. Project-pinned compatibility lock (best for repeated builds)

- A checked-in file that explicitly pins:
  - ESP-IDF version/tag
  - plugin/framework versions/tags/commits (ESP-ADF, ESP-SR, etc.)
  - source of compatibility proof (URL/release/matrix)
  - date/notes (optional)

Use `assets/templates/compatibility/` as a starting point.

## 2. Official compatibility matrices / release notes

- ESP-ADF README compatibility matrix (row for ADF release + column for exact IDF release)
- ESP-ADF release notes (exact supported IDF versions)
- ESP-SKAINET release/bundle docs (if using ESP-SR + audio stack combinations)

## 3. Component manifests / dependency constraints

- `idf_component.yml` `dependencies.idf` version range (for example ESP-SR)
- Managed component lockfiles and version pins

Note:
- Manifest ranges are useful evidence for plugin -> IDF compatibility.
- They are usually not enough to prove plugin A <-> plugin B compatibility.

## 4. Local, reproducible build/test evidence

- Successful clean build with exact versions
- Smoke test or sample example build
- Preferably with the compatibility evidence recorded afterward in a lock file

Build success alone is helpful but should not replace upstream version evidence when known compatibility matrices exist.

## Required Checks for Common Stacks

### ESP-IDF + ESP-ADF

- Record exact ESP-IDF version (major.minor.patch or tag)
- Record exact ESP-ADF version/tag
- Verify the ESP-ADF README/release matrix explicitly lists the selected ESP-IDF version
- Verify the selected ADF row marks it as supported

If the exact IDF release is not listed, compatibility is unproven (do not assume forward compatibility).

### ESP-IDF + ESP-SR

- Record exact ESP-IDF version
- Record exact ESP-SR version/tag
- Verify `esp-sr/idf_component.yml` `dependencies.idf` range includes the selected ESP-IDF version
- Check additional target constraints (chip support, PSRAM recommendations, etc.) from ESP-SR docs

### ESP-IDF + ESP-ADF + ESP-SR (Cross-Stack)

- Pass all individual checks above
- Also require explicit cross-stack evidence:
  - project compatibility lock file, or
  - ESP-SKAINET release/bundle documentation, or
  - user-provided tested matrix with exact versions

Do not infer cross-stack compatibility from two independent checks.

## Agent Workflow

1. Enumerate plugins/frameworks in use and exact versions.
2. Gather evidence from local manifests/readmes/release docs.
3. Write an evidence report (and optionally a lock file) before build.
4. If any edge is missing/ambiguous, stop and ask for version changes or approved evidence.

Reference helper:
- `scripts/check_plugin_compatibility.py` validates common ESP-IDF/ESP-ADF/ESP-SR evidence and writes `build/plugin-compatibility-evidence.txt`.
- Set `ESP_REQUIRED_PLUGINS=esp-adf,esp-sr` to force checks when auto-detection is uncertain.
- Set `ESP_STACK_COMPAT_EVIDENCE=...` or add a project compatibility lock file to satisfy cross-stack proof requirements.

## Example: What "Unproven" Looks Like

- ESP-ADF matrix supports up to IDF `v5.3`, but project is on IDF `v5.5`.
- ESP-SR manifest says `idf >= 5.0` and passes.
- Result: stack is still unproven because ADF -> IDF evidence is missing for `v5.5`.

## Review Checklist

- Every framework in use has an exact version/tag/commit recorded.
- Each framework has explicit compatibility evidence against the exact ESP-IDF version.
- Cross-stack evidence exists when multiple frameworks interact.
- Evidence is written to a report or lock file before build.
- No “probably compatible” assumptions remain.
