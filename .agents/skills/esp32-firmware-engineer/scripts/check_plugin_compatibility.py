#!/usr/bin/env python3
"""Validate ESP-IDF plugin/framework compatibility with concrete evidence.

Evidence sources (local, machine-readable where possible):
- ESP-IDF version from `idf.py --version`
- ESP-ADF compatibility matrix in `README.md`
- ESP-SR IDF dependency range in `idf_component.yml`
- Optional cross-stack evidence file/env when ESP-ADF + ESP-SR are both used
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
import urllib.request
import urllib.error
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_REPORT = "plugin-compatibility-evidence.txt"


@dataclass
class CheckResult:
    ok: bool
    summary: str
    details: list[str]


def run(cmd: list[str], cwd: Path | None = None) -> tuple[int, str]:
    proc = subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    return proc.returncode, proc.stdout.strip()


def parse_semver(text: str) -> tuple[int, int, int]:
    m = re.search(r"\bv?(\d+)\.(\d+)(?:\.(\d+))?\b", text)
    if not m:
        raise ValueError(f"Could not parse version from: {text!r}")
    return int(m.group(1)), int(m.group(2)), int(m.group(3) or 0)


def semver_cmp(a: tuple[int, int, int], b: tuple[int, int, int]) -> int:
    return (a > b) - (a < b)


def eval_constraints(version: tuple[int, int, int], expr: str) -> bool:
    # Supports simple comma-separated ranges like ">=5.0,<5.6" or ">=5.0"
    parts = [p.strip() for p in expr.split(",") if p.strip()]
    for part in parts:
        m = re.fullmatch(r"(>=|<=|>|<|=)\s*v?(\d+\.\d+(?:\.\d+)?)", part)
        if not m:
            # Unknown constraint syntax => cannot claim evidence
            return False
        op = m.group(1)
        rhs = parse_semver(m.group(2))
        c = semver_cmp(version, rhs)
        if op == ">=" and c < 0:
            return False
        if op == "<=" and c > 0:
            return False
        if op == ">" and c <= 0:
            return False
        if op == "<" and c >= 0:
            return False
        if op == "=" and c != 0:
            return False
    return True


def idf_version(idf_py: str) -> tuple[tuple[int, int, int], str]:
    rc, out = run([idf_py, "--version"])
    if rc != 0:
        raise RuntimeError(f"idf.py --version failed:\n{out}")
    return parse_semver(out), out


def git_describe(repo: Path) -> str:
    rc, out = run(["git", "-C", str(repo), "describe", "--tags", "--always", "--dirty"])
    if rc != 0:
        raise RuntimeError(f"git describe failed for {repo}:\n{out}")
    return out


def detect_used_plugins(project_dir: Path) -> set[str]:
    explicit = os.getenv("ESP_REQUIRED_PLUGINS", "").strip()
    if explicit:
        plugins = {p.strip().lower() for p in explicit.split(",") if p.strip()}
        return plugins

    # Heuristic scan of common source/config files (best effort, conservative)
    patterns = {
        "esp-adf": re.compile(r"\b(audio_pipeline|audio_board|audio_element|esp-adf)\b", re.I),
        "esp-sr": re.compile(r"\b(esp-sr|esp_afe|wakenet|multinet|esp speech recognition)\b", re.I),
    }
    hits: set[str] = set()
    ex_dirs = {".git", "build", "managed_components"}
    for path in project_dir.rglob("*"):
        if any(part in ex_dirs for part in path.parts):
            continue
        if not path.is_file():
            continue
        if path.stat().st_size > 512 * 1024:
            continue
        if path.suffix.lower() not in {
            ".c", ".cc", ".cpp", ".h", ".hpp", ".cmake", ".yml", ".yaml", ""
        }:
            continue
        try:
            text = path.read_text(errors="ignore")
        except Exception:
            continue
        for plugin, pat in patterns.items():
            if plugin not in hits and pat.search(text):
                hits.add(plugin)
    return hits


def resolve_repo(candidates: Iterable[str]) -> Path | None:
    for c in candidates:
        if not c:
            continue
        p = Path(c).expanduser()
        if p.is_dir():
            return p
    return None


_ADF_README_URLS = [
    # master branch README is always current; also try the stable branch tag if known
    "https://raw.githubusercontent.com/espressif/esp-adf/master/README.md",
]
_ADF_FETCH_TIMEOUT = 10  # seconds


def fetch_adf_readme_online(details: list[str]) -> str | None:
    """Fetch the ESP-ADF README.md from GitHub. Returns text or None on failure."""
    for url in _ADF_README_URLS:
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "esp-idf-compat-checker/1.0"})
            with urllib.request.urlopen(req, timeout=_ADF_FETCH_TIMEOUT) as resp:
                text = resp.read().decode("utf-8", errors="ignore")
            details.append(f"Fetched ESP-ADF README from {url}")
            return text
        except urllib.error.URLError as exc:
            details.append(f"Could not fetch ADF README from {url}: {exc}")
        except Exception as exc:
            details.append(f"Unexpected error fetching ADF README: {exc}")
    return None


def _parse_adf_matrix(
    text: str,
    idf_ver: tuple[int, int, int],
    adf_version_hint: str,
    details: list[str],
) -> CheckResult:
    """Parse the ADF compatibility matrix table from README text."""
    lines = text.splitlines()

    header_line = next((ln for ln in lines if ln.startswith("|                       | ESP-IDF")), None)
    if not header_line:
        return CheckResult(False, "ESP-ADF compatibility table header not found in README", details)

    idf_minor = f"{idf_ver[0]}.{idf_ver[1]}"
    header_cells = [c.strip() for c in header_line.strip().strip("|").split("|")]
    col_index = None
    for idx, cell in enumerate(header_cells):
        if f"Release/v{idf_minor}" in cell:
            col_index = idx
            break
    if col_index is None:
        details.append(f"ADF README matrix header: {header_line}")
        return CheckResult(
            False,
            f"ESP-ADF README matrix does not list ESP-IDF v{idf_minor} — version may be too new or old",
            details,
        )

    row_key = None
    m_rel = re.match(r"v(\d+\.\d+)", adf_version_hint.split("-")[0])
    if m_rel:
        row_key = f"ESP-ADF <br> Release/v{m_rel.group(1)}"
    elif "master" in adf_version_hint.lower():
        row_key = "ESP-ADF <br> Master"

    if not row_key:
        details.append(f"ADF version hint: {adf_version_hint}")
        return CheckResult(False, "Cannot map ESP-ADF version to matrix row; need explicit compatibility evidence", details)

    row_line = next((ln for ln in lines if row_key in ln), None)
    if not row_line:
        details.append(f"Expected row key: {row_key}")
        return CheckResult(False, "ESP-ADF matrix row for current version not found", details)

    row_cells = [c.strip() for c in row_line.strip().strip("|").split("|")]
    if col_index >= len(row_cells):
        details.append(f"Row line: {row_line}")
        return CheckResult(False, "ESP-ADF matrix row parse failed (column mismatch)", details)

    cell = row_cells[col_index]
    details.append(f"ADF matrix row: {row_line}")
    details.append(f"ADF matrix cell for ESP-IDF v{idf_minor}: {cell}")
    if "yes-checkm" in cell or ('"supported"' in cell and "no-icon" not in cell):
        return CheckResult(True, f"ESP-ADF matrix explicitly supports ESP-IDF v{idf_minor}", details)
    if "no-icon" in cell or "not supported" in cell:
        return CheckResult(False, f"ESP-ADF matrix marks ESP-IDF v{idf_minor} as not supported", details)
    return CheckResult(False, "ESP-ADF matrix cell is ambiguous; need explicit evidence", details)


def adf_check(idf_ver: tuple[int, int, int], adf_dir: Path | None) -> CheckResult:
    details: list[str] = []

    # Determine ADF version string (from local clone or env hint)
    adf_version_hint = os.getenv("ESP_ADF_VERSION", "").strip()
    if adf_dir is not None:
        try:
            adf_version_hint = git_describe(adf_dir)
            details.append(f"ESP-ADF git describe: {adf_version_hint}")
        except Exception as exc:
            details.append(f"git describe failed for ESP-ADF: {exc}")

    if not adf_version_hint:
        return CheckResult(
            False,
            "ESP-ADF version unknown. Set ESP_ADF_VERSION=v2.7 or provide a local ADF clone.",
            details,
        )

    # Try local README first
    if adf_dir is not None:
        readme = adf_dir / "README.md"
        if readme.exists():
            details.append(f"Using local ESP-ADF README: {readme}")
            text = readme.read_text(errors="ignore")
            return _parse_adf_matrix(text, idf_ver, adf_version_hint, details)
        details.append(f"Local ESP-ADF README not found at {readme}; trying online source")

    # Fall back to fetching from GitHub
    details.append("No local ESP-ADF clone available; fetching compatibility matrix from GitHub")
    text = fetch_adf_readme_online(details)
    if text is None:
        return CheckResult(
            False,
            "Could not retrieve ESP-ADF compatibility matrix (no local clone, network unavailable). "
            "Set ESP_ADF_DIR or ADF_PATH, or set ESP_ADF_VERSION and provide ESP_STACK_COMPAT_EVIDENCE.",
            details,
        )
    return _parse_adf_matrix(text, idf_ver, adf_version_hint, details)


def sr_check(idf_ver: tuple[int, int, int], sr_dir: Path) -> CheckResult:
    details: list[str] = []
    describe = git_describe(sr_dir)
    details.append(f"ESP-SR git describe: {describe}")

    manifest = sr_dir / "idf_component.yml"
    if not manifest.exists():
        return CheckResult(False, "ESP-SR idf_component.yml not found (no dependency evidence)", details)
    text = manifest.read_text(errors="ignore")

    version_match = re.search(r'(?m)^version:\s*"([^"]+)"', text)
    if version_match:
        details.append(f"ESP-SR manifest version: {version_match.group(1)}")

    # Capture dependency range with indentation under dependencies:
    dep_match = re.search(r'(?ms)^dependencies:\s*\n(?:^[ \t].*\n)*?[ \t]+idf:\s*"([^"]+)"', text)
    if not dep_match:
        dep_match = re.search(r'(?m)^[ \t]*idf:\s*"([^"]+)"', text)
    if not dep_match:
        return CheckResult(False, "ESP-SR idf dependency range not found in manifest", details)

    constraint = dep_match.group(1).strip()
    details.append(f"ESP-SR manifest idf dependency: {constraint}")
    if eval_constraints(idf_ver, constraint):
        return CheckResult(True, f"ESP-SR manifest constraint '{constraint}' matches ESP-IDF version", details)
    return CheckResult(False, f"ESP-SR manifest constraint '{constraint}' does not match ESP-IDF version", details)


def cross_stack_check(project_dir: Path, used: set[str], versions: dict[str, str]) -> CheckResult:
    details: list[str] = []
    if not {"esp-adf", "esp-sr"}.issubset(used):
        return CheckResult(True, "No ADF+SR cross-stack evidence required (both not in use together)", details)

    env_evidence = os.getenv("ESP_STACK_COMPAT_EVIDENCE", "").strip()
    if env_evidence:
        details.append(f"ESP_STACK_COMPAT_EVIDENCE={env_evidence}")
        return CheckResult(True, "Cross-stack evidence provided via ESP_STACK_COMPAT_EVIDENCE", details)

    candidates = [
        project_dir / "esp-framework-compat.lock",
        project_dir / "esp-framework-compatibility.lock",
        project_dir / "docs" / "esp-framework-compat.md",
        project_dir / "docs" / "compatibility" / "esp-frameworks.md",
    ]
    for p in candidates:
        if p.exists():
            txt = p.read_text(errors="ignore")
            # Require explicit mention of all frameworks/versions to count as evidence.
            ok = True
            for key, val in versions.items():
                if key in ("esp-idf", "esp-adf", "esp-sr") and val:
                    if key not in txt.lower() or val.split("-")[0] not in txt:
                        ok = False
                        break
            details.append(f"Found cross-stack evidence file: {p}")
            if ok:
                return CheckResult(True, f"Cross-stack evidence file found: {p}", details)
            return CheckResult(False, f"Cross-stack evidence file {p} exists but does not explicitly pin all framework versions", details)

    return CheckResult(
        False,
        "ADF + SR are both in use but no explicit cross-stack compatibility evidence was provided",
        details,
    )


def write_report(report_path: Path, lines: list[str]) -> None:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(lines) + "\n")


def main() -> int:
    project_dir = Path(os.getenv("PROJECT_DIR", ".")).resolve()
    build_dir = Path(os.getenv("BUILD_DIR", project_dir / "build"))
    report_path = build_dir / DEFAULT_REPORT
    idf_py = os.getenv("IDF_PY", "idf.py")

    report: list[str] = []
    report.append(f"project_dir={project_dir}")

    try:
        idf_ver, idf_ver_text = idf_version(idf_py)
    except Exception as exc:
        report.append(f"[FAIL] ESP-IDF preflight failed: {exc}")
        write_report(report_path, report)
        print("\n".join(report))
        return 1

    idf_minor = f"{idf_ver[0]}.{idf_ver[1]}"
    report.append(f"esp-idf={idf_ver_text}")
    report.append(f"esp-idf-semver={idf_ver[0]}.{idf_ver[1]}.{idf_ver[2]}")

    used = detect_used_plugins(project_dir)
    report.append(f"used-plugins={'none' if not used else ','.join(sorted(used))}")

    versions: dict[str, str] = {"esp-idf": f"v{idf_minor}"}
    failures = 0

    if "esp-adf" in used:
        adf_dir = resolve_repo(
            [os.getenv("ESP_ADF_DIR", ""), os.getenv("ADF_PATH", ""), str(Path.home() / "esp/esp-adf")]
        )
        if adf_dir:
            report.append(f"esp-adf-dir={adf_dir}")
        else:
            report.append("esp-adf-dir=none (will attempt online compatibility check)")
        res = adf_check(idf_ver, adf_dir)
        if adf_dir:
            try:
                versions["esp-adf"] = git_describe(adf_dir)
            except Exception:
                versions["esp-adf"] = os.getenv("ESP_ADF_VERSION", "")
        else:
            versions["esp-adf"] = os.getenv("ESP_ADF_VERSION", "")
        report.extend([f"  {d}" for d in res.details])
        report.append(("[OK] " if res.ok else "[FAIL] ") + res.summary)
        if not res.ok:
            failures += 1

    if "esp-sr" in used:
        sr_dir = resolve_repo(
            [os.getenv("ESP_SR_DIR", ""), os.getenv("ESP_SR_PATH", ""), str(Path.home() / "esp/esp-sr")]
        )
        if not sr_dir:
            report.append("[FAIL] esp-sr is used but ESP_SR_DIR/ESP_SR_PATH/local repo was not found")
            failures += 1
        else:
            report.append(f"esp-sr-dir={sr_dir}")
            res = sr_check(idf_ver, sr_dir)
            try:
                versions["esp-sr"] = git_describe(sr_dir)
            except Exception:
                versions["esp-sr"] = ""
            report.extend([f"  {d}" for d in res.details])
            report.append(("[OK] " if res.ok else "[FAIL] ") + res.summary)
            if not res.ok:
                failures += 1

    if used:
        cross = cross_stack_check(project_dir, used, versions)
        report.extend([f"  {d}" for d in cross.details])
        report.append(("[OK] " if cross.ok else "[FAIL] ") + cross.summary)
        if not cross.ok:
            failures += 1
    else:
        report.append("[OK] No known plugin frameworks detected; no plugin compatibility checks required.")
        report.append("      Set ESP_REQUIRED_PLUGINS=esp-adf,esp-sr to force checks.")

    report.append(f"result={'PASS' if failures == 0 else 'FAIL'}")
    write_report(report_path, report)
    print("\n".join(report))
    return 0 if failures == 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())
