#!/usr/bin/env python3
"""Generate deterministic NotePeek performance smoke-test fixtures.

The script intentionally does not launch the GUI. It creates text, Markdown,
and directory-tree fixtures that can be used for manual or automated launch
timing around NotePeek builds.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def write_repeated(path: Path, target_size: int, line: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    encoded = line.encode("utf-8")
    with path.open("wb") as handle:
        written = 0
        while written < target_size:
            chunk = encoded[: max(0, min(len(encoded), target_size - written))]
            handle.write(chunk)
            written += len(chunk)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default="build/perf-fixtures", help="fixture output directory")
    args = parser.parse_args()

    out = Path(args.out)
    sizes = {
        "1mb": 1 * 1024 * 1024,
        "10mb": 10 * 1024 * 1024,
        "50mb": 50 * 1024 * 1024,
    }

    for label, size in sizes.items():
        write_repeated(out / f"text-{label}.txt", size, "0123456789 abcdefghijklmnopqrstuvwxyz\n")
        write_repeated(out / f"markdown-{label}.md", size, "## Heading\n\n- item one\n- item two\n\n")

    tree = out / "large-tree"
    for directory in range(100):
        for file_index in range(20):
            path = tree / f"dir-{directory:03d}" / f"file-{file_index:03d}.txt"
            write_repeated(path, 1024, f"{directory}:{file_index} fixture line\n")

    print(f"Wrote fixtures to {out.resolve()}")


if __name__ == "__main__":
    main()
