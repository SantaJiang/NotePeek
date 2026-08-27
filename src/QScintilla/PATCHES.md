# NotePeek Changes to QScintilla 2.14.1

All entries below are modifications maintained by the NotePeek project and are
distributed under GNU GPL version 3.0 when they form part of QScintilla.

## Build and identity

- Replaced the upstream qmake-only library build with the project CMake target.
- Renamed the shipped binary to `NotePeekScintilla` and assigned an independent
  fork version and ABI.
- Added Qt 6 and cross-platform packaging integration.

## Lexer wrappers

- Added QML and Rust QScintilla wrapper classes for bundled Scintilla lexers.
- Moved the NotePeek language registry and Dockerfile, qmake, ignore-file,
  Groovy, Starlark, and Meson lexers into the fork library.

## Public NotePeek editor API

- Added `NotePeek::Editor`, an exported QScintilla subclass that owns reusable
  bracket assistance, folding commands, matching-brace navigation and
  document-word completion without depending on MainPage document state.
- Added an LSP-compatible semantic-token model, replaceable provider boundary,
  a lightweight built-in classifier and theme-aware semantic colour overlay.
- Added revisioned range deltas, cancellation and analysis budgets so clients
  can run providers asynchronously and apply only valid viewport results.

## Syntax themes

- Added semantic lexer-role mapping and a validated JSON theme engine.
- Added ten built-in syntax themes (five dark and five light), including sourced
  adaptations of VS Code, Qt Creator, Rosé Pine, and Ayu. Themes are independent
  JSON files, and validated external themes load dynamically from installation
  and per-user directories. See `NotePeek/themes/SOURCES.md` for upstream
  revisions, licenses, and the external-file rules.

## Rendering changes

- Centred line numbers in number margins used by NotePeek.
- Prevented word wrapping from splitting Markdown list-marker prefixes.
- Kept left and right text padding while extending caret-line backgrounds
  through that padding, including translucent highlights, so the highlight
  reaches both edges of the text area. Range invalidation includes the left
  padding so focus and caret changes cannot leave stale highlight pixels.
- Drew the caret line's number in the accent colour, following the caret line
  background.  `MarginView::PaintMargin` passes a foreground override to a new
  `DrawTextNoClipPhase` overload rather than recolouring a copied `Style`,
  because `Style`'s copy constructor deliberately leaves the font empty
  (`FontAlias` is non-assignable) and a copy would draw in a fallback font.
  `Editor::RedrawMarginForCaretLineChange` repaints the whole margin on a caret
  line change; it is called from the selection setters and again at the end of
  `Editor::MovedCaret`, after scrolling, because a per-line invalidation
  computed before the scroll can land outside the viewport and repaint nothing.
- Extended the caret-line background into the gutter: `MarginView` now fills
  the active line's cell across every gutter margin with the caret-line
  background (respecting the caret-line alpha and frame settings), and draws
  the line number transparently so it does not repaint over that background,
  keeping the highlighted number and fold markers on the same background as the
  text. `Editor::HorizontalMove` records the previous main caret so `MovedCaret`
  repaints the margin when a horizontal move crosses a line boundary, keeping
  that background on the right line.

These rendering changes predate the formal fork interface.  They will be
converted into opt-in public configuration while the NotePeek editor API is
introduced, so non-NotePeek QScintilla behavior is not changed globally.

## Performance contract

- Semantic results update only their requested line range; edits shift or
  invalidate cached tokens incrementally instead of clearing the document.
- Reapplying an unchanged semantic palette is a no-op.
- MainPage Release performance tests require same-feature scrolling to stay
  within 5% of native QScintilla and keep worker and UI-apply phases below 2 ms
  for the 1 MiB C++ fixture. Debug builds retain a 10% diagnostic guard for
  unoptimized Scintilla decoration traversal.
