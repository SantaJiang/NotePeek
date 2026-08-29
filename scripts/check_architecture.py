#!/usr/bin/env python3
"""Enforce dependency rules for modules that have completed migration."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)


def module_violations(root: Path, relative_module: str) -> list[str]:
    source_root = root / "src" / relative_module
    forbidden_includes = (
        "QWidget",
        "QtWidgets/",
        "Qsci/",
        "NotePeek/",
        "MainPage/",
        "presentation/",
        "app/",
    )
    violations: list[str] = []
    for path in sorted(source_root.rglob("*")):
        if path.suffix not in {".h", ".hpp", ".cpp", ".cc"}:
            continue
        contents = path.read_text(encoding="utf-8")
        for include in INCLUDE_PATTERN.findall(contents):
            crosses_feature_boundary = (
                include.startswith("features/")
                and not include.startswith(f"{relative_module}/")
            )
            if include.startswith(forbidden_includes) or crosses_feature_boundary:
                relative = path.relative_to(root)
                violations.append(f"{relative}: forbidden module include {include}")

    cmake_path = source_root / "CMakeLists.txt"
    cmake = cmake_path.read_text(encoding="utf-8")
    for dependency in ("Qt6::Widgets", "qscintilla2", "qlitehtml", "notepeek_core"):
        if dependency in cmake:
            violations.append(
                f"{cmake_path.relative_to(root)}: forbidden module dependency {dependency}"
            )
    return violations


def editor_kit_violations(root: Path) -> list[str]:
    """Keep application editor extensions out of the shared fork binary."""
    qscintilla_cmake_path = root / "src" / "QScintilla" / "CMakeLists.txt"
    editor_kit_cmake_path = root / "src" / "editor-kit" / "CMakeLists.txt"
    qscintilla_cmake = qscintilla_cmake_path.read_text(encoding="utf-8")
    editor_kit_cmake = editor_kit_cmake_path.read_text(encoding="utf-8")
    violations: list[str] = []

    required_qscintilla_exclusions = (
        'EXCLUDE REGEX "/notepeekeditor[.]cpp$"',
        'EXCLUDE REGEX "/notepeekeditor[.]h$"',
    )
    for exclusion in required_qscintilla_exclusions:
        if exclusion not in qscintilla_cmake:
            violations.append(
                f"{qscintilla_cmake_path.relative_to(root)}: missing editor-kit exclusion {exclusion}"
            )

    if "target_compile_definitions(qscintilla2 INTERFACE QSCINTILLA_DLL)" not in qscintilla_cmake:
        violations.append(
            f"{qscintilla_cmake_path.relative_to(root)}: shared-library import definition must propagate from qscintilla2"
        )

    required_editor_kit_ownership = (
        "add_library(notepeek_editor_kit STATIC",
        "NotePeek/*.cpp",
        "/notepeekeditor.cpp",
        "/NotePeek/notepeekthemes.qrc",
        "qscintilla2",
    )
    for ownership in required_editor_kit_ownership:
        if ownership not in editor_kit_cmake:
            violations.append(
                f"{editor_kit_cmake_path.relative_to(root)}: missing editor-kit ownership {ownership}"
            )

    application_headers = [root / "src" / "QScintilla" / "Qsci" / "notepeekeditor.h"]
    application_headers.extend(
        sorted((root / "src" / "QScintilla" / "NotePeek").rglob("*.h"))
    )
    for header in application_headers:
        if "QSCINTILLA_EXPORT" in header.read_text(encoding="utf-8"):
            violations.append(
                f"{header.relative_to(root)}: internal editor-kit API must not use QScintilla export macros"
            )
    return violations


def persistence_boundary_violations(root: Path) -> list[str]:
    """Keep core persistence independent from editor widgets and dialogs."""
    paths = (
        root / "src" / "MainPage" / "editor" / "documentpersistenceservice.h",
        root / "src" / "MainPage" / "editor" / "documentpersistenceservice.cpp",
        root / "src" / "MainPage" / "editor" / "documentencodingreloadservice.h",
        root / "src" / "MainPage" / "editor" / "documentencodingreloadservice.cpp",
    )
    forbidden_tokens = (
        "CustomEditor",
        "QFileDialog",
        "QMessageBox",
        "QWidget",
        "Qsci/",
    )
    violations: list[str] = []
    for path in paths:
        contents = path.read_text(encoding="utf-8")
        for token in forbidden_tokens:
            if token in contents:
                violations.append(
                    f"{path.relative_to(root)}: document IO boundary contains UI token {token}"
                )
    return violations


def legacy_foundation_header_violations(root: Path) -> list[str]:
    legacy_names = (
        "backgroundworkscheduler.h",
        "documentpath.h",
        "performanceconstants.h",
    )
    editor_root = root / "src" / "MainPage" / "editor"
    violations: list[str] = []
    for name in legacy_names:
        path = editor_root / name
        if path.exists():
            violations.append(
                f"{path.relative_to(root)}: legacy foundation forwarding header must stay removed"
            )

    legacy_include = re.compile(
        r'[<\"]editor/(?:backgroundworkscheduler|documentpath|performanceconstants)[.]h[>\"]'
    )
    for source_root in (root / "src", root / "tests"):
        for path in sorted(source_root.rglob("*")):
            if path.suffix not in {".h", ".hpp", ".cpp", ".cc"}:
                continue
            if legacy_include.search(path.read_text(encoding="utf-8")):
                violations.append(
                    f"{path.relative_to(root)}: include foundation API through foundation/"
                )
    return violations


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()

    root = args.root.resolve()
    violations = module_violations(root, "foundation")
    violations.extend(module_violations(root, "features/settings"))
    violations.extend(editor_kit_violations(root))
    violations.extend(persistence_boundary_violations(root))
    violations.extend(legacy_foundation_header_violations(root))
    if violations:
        print("Architecture dependency violations:")
        for violation in violations:
            print(f"- {violation}")
        return 1
    print("Architecture dependency rules passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
