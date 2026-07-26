#!/usr/bin/env python3
"""Run a CI command and expose useful tail output as a check annotation."""

from __future__ import annotations

import argparse
from collections import deque
import subprocess
import sys


def annotation_escape(message: str) -> str:
    return (
        message.replace("%", "%25")
        .replace("\r", "%0D")
        .replace("\n", "%0A")
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--title", required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()

    command = args.command
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        parser.error("a command is required after --")

    tail: deque[str] = deque(maxlen=40)
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
    )
    assert process.stdout is not None
    for line in process.stdout:
        print(line, end="", flush=True)
        tail.append(line)

    return_code = process.wait()
    if return_code:
        details = "".join(tail).strip()
        print(f"::error title={args.title}::{annotation_escape(details)}")
    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
