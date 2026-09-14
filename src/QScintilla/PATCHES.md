# NotePeek Changes to QScintilla 2.14.1

All entries below are modifications maintained by the NotePeek project and are
distributed under GNU GPL version 3.0 when they form part of QScintilla.

## Build and identity

- Replaced the upstream qmake-only library build with the project CMake target.
- Renamed the shipped binary to `NotePeekScintilla` and assigned an independent
  fork version and ABI.
- Added Qt 6 and cross-platform packaging integration.
- Split NotePeek-specific editor APIs, lexers, semantic services, and theme
  resources into the application-owned static `notepeek_editor_kit` target.
  The shared fork binary now contains only QScintilla and Scintilla code, so
  application extensions have one build owner on every platform.

## Lexer wrappers

- Added QML and Rust QScintilla wrapper classes for bundled Scintilla lexers.
- Added the NotePeek language registry and Dockerfile, qmake, ignore-file,
  Groovy, Starlark, and Meson lexers alongside the fork sources. They are built
  by `notepeek_editor_kit`, not exported by the shared fork binary.

## Internal NotePeek editor API

- Added `NotePeek::Editor`, an application-level QScintilla subclass that owns reusable
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
- When a visible number margin is present and the text is horizontally scrolled
  away from its left edge, draw a one-pixel separator in the themed indentation-
  guide colour at the right edge of the fixed gutter. The separator disappears at
  horizontal offset zero and when the number margin is hidden.

These rendering changes predate the formal fork interface.  They will be
converted into opt-in public configuration while the NotePeek editor API is
introduced, so non-NotePeek QScintilla behavior is not changed globally.

## Input method

`inputMethodQuery()` only answered `Qt::ImCursorRectangle`, `Qt::ImFont`,
`Qt::ImCursorPosition`, `Qt::ImSurroundingText` and `Qt::ImCurrentSelection`, and
answered the first of those two positional queries in Scintilla's UTF-8 byte
offsets rather than the UTF-16 code units Qt specifies.  Platform plugins build
everything else from those answers, so an input method was handed a caret outside
its own surrounding text, an empty context window and a selection range whose
location had wrapped negative.  On macOS that made `attributedSubstringForProposedRange:`
return `nil` and `selectedRange` return garbage, so the Pinyin IME — which does
its smart replacement of quotes from exactly that context — could not see what
preceded the caret and emitted an opening `“` on every keystroke.

- `Qt::ImCursorPosition` reports a UTF-16 index into `Qt::ImSurroundingText`.
- `Qt::ImAbsolutePosition` and `Qt::ImAnchorPosition` are now supported, in the
  same UTF-16 units and, for the anchor, the same paragraph origin Qt uses.
- `Qt::ImTextBeforeCursor` and `Qt::ImTextAfterCursor` report up to the 1024
  characters QWidgetTextControl defaults to, windowed with
  `Document::GetRelativePositionUTF16()` so the cost stays bounded.
- `Qt::ImSurroundingText` no longer appends the buffer's padding NUL byte.
- `preeditPos` is cleared as soon as a composition commits, is replaced or is
  cancelled, and every input method event refreshes the micro focus.  Previously
  the first composition pinned `Qt::ImCursorRectangle` to a stale document
  position for the rest of the session, so the platform was told the caret never
  moved.
- UTF-16 offsets are counted from lead bytes instead of decoded into a `QString`,
  and `Qt::ImAbsolutePosition` reads the document in 64 KiB chunks.  That query
  is O(bytes before the caret); it is the one input method query whose cost still
  scales with the document.

## Performance contract

- Semantic results update only their requested line range; edits shift or
  invalidate cached tokens incrementally instead of clearing the document.
- Reapplying an unchanged semantic palette is a no-op.
- MainPage Release performance tests require same-feature scrolling to stay
  within 5% of native QScintilla and keep worker and UI-apply phases below 2 ms
  for the 1 MiB C++ fixture. Debug builds retain a 10% diagnostic guard for
  unoptimized Scintilla decoration traversal.
