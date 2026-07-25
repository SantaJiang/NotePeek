# Vendored Dependency Policy

NotePeek vendors its editor and Markdown stack to keep releases reproducible.
Vendored source is isolated under `src/QScintilla` and `src/third_party`; local
changes should be exceptional and documented in the updating commit.

## Updating a dependency

1. Record the upstream release or commit and review its license files.
2. Replace only the dependency directory. Do not mix product changes into the
   same commit.
3. Update `THIRD_PARTY_NOTICES.md`, including nested dependency versions.
4. Inspect new compiler warnings. NotePeek-owned code must remain clean with
   `NOTEPEEK_WARNINGS_AS_ERRORS=ON`; upstream warnings are not silently patched
   unless they indicate a correctness or security issue.
5. Run strict Debug and Release builds, all tests, sanitizer tests, the install
   verifier, and package smoke tests on macOS, Windows, and Linux.
6. Exercise Markdown rendering and PDF export with links, tables, task lists,
   Unicode, and unsafe HTML fixtures.

## Current warning baseline

AppleClang currently reports upstream warnings in Gumbo and litehtml, including
a pointer-to-enum cast in Gumbo and incomplete enum switches in litehtml. These
targets do not inherit NotePeek's `-Werror` policy. Any warning originating in
`src/MainPage`, `tests`, or `scripts` is a regression and must be fixed.

Review the dependency inventory at least once per minor NotePeek release and
immediately when an upstream security advisory affects a bundled component.
