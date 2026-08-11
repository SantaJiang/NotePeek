#!/usr/bin/env python3
"""Verify that packaged binaries honor their declared platform baselines."""

from __future__ import annotations

import argparse
import re
import subprocess
from pathlib import Path


def version(value: str) -> tuple[int, ...]:
    return tuple(int(component) for component in value.split("."))


def command_output(*command: str) -> str:
    return subprocess.run(
        command,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    ).stdout


def verify_macos(root: Path, expected: str) -> None:
    candidates = [path for path in root.rglob("*") if path.is_file()]
    checked = 0
    failures: list[str] = []
    for path in candidates:
        try:
            output = command_output("otool", "-l", str(path))
        except subprocess.CalledProcessError:
            continue
        minimums = re.findall(r"^\s+minos\s+([0-9.]+)$", output, re.MULTILINE)
        minimums += re.findall(
            r"cmd LC_VERSION_MIN_MACOSX.*?^\s+version\s+([0-9.]+)$",
            output,
            re.MULTILINE | re.DOTALL,
        )
        if not minimums:
            continue
        checked += 1
        too_new = [item for item in minimums if version(item) > version(expected)]
        if too_new:
            failures.append(f"{path}: requires macOS {max(too_new, key=version)}")
    if checked == 0:
        raise SystemExit(f"No Mach-O binaries found under {root}")
    if failures:
        raise SystemExit(
            f"Mach-O deployment target exceeds declared macOS {expected}:\n"
            + "\n".join(failures)
        )
    print(f"Verified {checked} Mach-O binaries against macOS {expected}")


def verify_linux(root: Path, declared: str) -> None:
    candidates = [path for path in root.rglob("*") if path.is_file()]
    checked = 0
    maximum = (0,)
    maximum_text = "0"
    for path in candidates:
        try:
            output = command_output("readelf", "--version-info", str(path))
        except subprocess.CalledProcessError:
            continue
        requirements = re.findall(r"GLIBC_([0-9]+(?:\.[0-9]+)+)", output)
        if not requirements:
            continue
        checked += 1
        candidate = max(requirements, key=version)
        if version(candidate) > maximum:
            maximum = version(candidate)
            maximum_text = candidate
    if checked == 0:
        raise SystemExit(f"No ELF binaries with glibc requirements found under {root}")
    if maximum > version(declared):
        raise SystemExit(
            f"ELF binaries require GLIBC_{maximum_text}, exceeding package declaration {declared}"
        )
    print(f"Verified {checked} ELF binaries: maximum GLIBC_{maximum_text} <= {declared}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--macos", metavar="VERSION")
    group.add_argument("--glibc", metavar="VERSION")
    args = parser.parse_args()
    root = args.root.resolve()
    if args.macos:
        verify_macos(root, args.macos)
    else:
        verify_linux(root, args.glibc)


if __name__ == "__main__":
    main()
