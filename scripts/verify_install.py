#!/usr/bin/env python3
"""Verify that an installed NotePeek tree contains its runnable artifacts."""

from __future__ import annotations

import argparse
import platform
from pathlib import Path


def require_file(path: Path) -> None:
    if not path.is_file() or path.stat().st_size == 0:
        raise SystemExit(f"Missing or empty installed artifact: {path}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("prefix", type=Path)
    parser.add_argument("--system", choices=("Darwin", "Windows", "Linux"), default=platform.system())
    args = parser.parse_args()
    prefix = args.prefix.resolve()

    if args.system == "Darwin":
        require_file(prefix / "notepeek.app" / "Contents" / "MacOS" / "notepeek")
        require_file(prefix / "notepeek.app" / "Contents" / "Resources" / "THIRD_PARTY_NOTICES.md")
        libraries = list((prefix / "notepeek.app" / "Contents" / "Frameworks").glob("libqscintilla2*.dylib"))
    elif args.system == "Windows":
        require_file(prefix / "bin" / "notepeek.exe")
        require_file(prefix / "share" / "notepeek" / "THIRD_PARTY_NOTICES.md")
        libraries = list((prefix / "bin").glob("qscintilla2*.dll"))
    else:
        require_file(prefix / "bin" / "notepeek")
        require_file(prefix / "share" / "notepeek" / "THIRD_PARTY_NOTICES.md")
        libraries = list((prefix / "lib" / "notepeek").glob("libqscintilla2.so*"))
        libraries += list((prefix / "lib64" / "notepeek").glob("libqscintilla2.so*"))

    if not any(path.is_file() and path.stat().st_size > 0 for path in libraries):
        raise SystemExit("The installed QScintilla runtime was not found")
    print(f"Verified NotePeek installation at {prefix}")


if __name__ == "__main__":
    main()
