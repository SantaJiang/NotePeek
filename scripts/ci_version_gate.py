#!/usr/bin/env python3
"""Decide whether a push should run the expensive CI jobs."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys


VERSION_PATTERN = re.compile(
    r"project\s*\(\s*NotePeek\b.*?\bVERSION\s+(\d+)\.(\d+)\.(\d+)",
    re.DOTALL,
)
ZERO_SHA = "0" * 40
CI_INFRASTRUCTURE_PATHS = {
    ".github/workflows/ci.yml",
    ".github/workflows/release.yml",
    "scripts/ci_command.py",
    "scripts/ci_version_gate.py",
    "scripts/verify_install.py",
    "tests/performance_test.cpp",
}
# Versions at or below this line predate automatic releases. This prevents the
# initial workflow rollout from publishing an existing, untagged version.
AUTO_RELEASE_BASELINE = (1, 3)


def git(*args: str) -> str:
    completed = subprocess.run(
        ["git", *args],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return completed.stdout


def project_version(revision: str) -> tuple[int, int, int]:
    contents = git("show", f"{revision}:CMakeLists.txt")
    match = VERSION_PATTERN.search(contents)
    if not match:
        raise ValueError(f"Could not find NotePeek's semantic version at {revision}")
    return tuple(int(component) for component in match.groups())


def changed_paths(before: str, after: str) -> set[str]:
    return set(filter(None, git("diff", "--name-only", before, after).splitlines()))


def minor_line_has_release_tag(version: tuple[int, int, int]) -> bool:
    tag_pattern = re.compile(
        rf"^[vV]{version[0]}\.{version[1]}\.\d+(?:[-+].*)?$"
    )
    return any(tag_pattern.match(tag) for tag in git("tag", "--list").splitlines())


def emit(run_ci: bool, release: bool, version: str, reason: str) -> None:
    print(f"run_ci={'true' if run_ci else 'false'}")
    print(f"release={'true' if release else 'false'}")
    print(f"version={version}")
    print(f"reason={reason}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--before", required=True, help="Git revision before the push")
    parser.add_argument("--after", required=True, help="Git revision after the push")
    args = parser.parse_args()

    try:
        after_version = project_version(args.after)
        version = ".".join(str(component) for component in after_version)

        if args.before == ZERO_SHA:
            emit(True, False, version, "new branch or repository history")
            return 0

        paths = changed_paths(args.before, args.after)
        before_version = project_version(args.before)
    except (subprocess.CalledProcessError, ValueError) as error:
        print(str(error), file=sys.stderr)
        emit(True, False, "", "version comparison unavailable; fail open")
        return 0

    if after_version[:2] > before_version[:2]:
        emit(
            True,
            True,
            version,
            f"major/minor version changed from {before_version[0]}.{before_version[1]} "
            f"to {after_version[0]}.{after_version[1]}",
        )
    elif before_version[:2] != after_version[:2]:
        emit(
            True,
            False,
            version,
            f"major/minor version did not increase: {before_version[0]}.{before_version[1]} "
            f"to {after_version[0]}.{after_version[1]}",
        )
    elif (
        after_version[:2] > AUTO_RELEASE_BASELINE
        and not minor_line_has_release_tag(after_version)
    ):
        emit(
            True,
            True,
            version,
            f"retrying unpublished {after_version[0]}.{after_version[1]} release",
        )
    elif paths & CI_INFRASTRUCTURE_PATHS:
        emit(True, False, version, "CI infrastructure changed")
    else:
        emit(
            False,
            False,
            version,
            f"major/minor version remains {after_version[0]}.{after_version[1]}",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
