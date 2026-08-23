# NotePeek Architecture

NotePeek is a Qt Widgets desktop editor built around a small set of UI composition classes and editor services.

## Top-Level Layout

```text
NotePeek/
├── deploy/                 Platform packaging assets
├── docs/                   Project and architecture notes
├── i18n/                   Translation sources
└── src/
    ├── MainPage/           Application code
    ├── QScintilla/         Embedded editor dependency
    └── third_party/        Embedded third-party libraries
```

## Runtime Composition

```text
main.cpp
  -> MainWindow
      -> DocumentSaveCoordinator (application-scoped save ordering/state)
      -> ExternalFileWatcher (application-scoped watched-path union)
      -> FileExplorerDock
      -> SearchResultsDock ──> FileSearcher (project-wide search, off-thread)
      -> QList<DocumentController> ──┐ one per EditorPane, in pane order
      -> SplitEditorCoordinator    ──┘ each controller owns:
          (synchronizes the             -> EditorManager
           controllers/panes)           -> DocumentPersistenceService
      -> FindReplaceManager
      -> HeaderSourceManager
      -> MarkdownPreviewer

BackgroundWorkScheduler
  -> interactive pool (formatting, search, semantic analysis)
  -> IO pool (load, save, external reload)
  -> background pool (Markdown rendering, file-type filtering)

GitRepositoryService
  -> GitCommandRunner (asynchronous system-Git processes)
  -> GitStatusParser (porcelain-v2 status snapshots)

EditorPane (view)
  -> DocumentPage (one per tab, plain or Markdown-capable)
```

## Module Responsibilities

### `MainWindow`

Owns the application shell: menus, dock placement, the find/replace widget, active editor pane routing, and top-level commands. It should not own file-system model details or document persistence details.

Members used by `updateToolActions()` (such as the Tools ▸ Encoding menu) must be constructed before the first call, which happens during `applyActiveController()` in the constructor.

### `widgets/FileExplorerDock`

Owns the file-system browser dock, including the `QFileSystemModel`, file-type filter proxy, tree view behavior, root directory changes, and file selection.

### `widgets/SearchResultsDock` and `find/FileSearcher`

`SearchResultsDock` is the project-wide search UI: query and exclude-pattern
rows, status line, and a file-grouped result tree that emits `matchActivated`
for jump-to-match. `FileSearcher` walks the directory tree and matches text on
an interactive worker. It shares `ProjectPathFilter` with the file tree, honors
the project-root `.gitignore` plus user-supplied exclusion globs, filters
candidates through `SupportedFilePolicy`, and caps results at
`FileSearcher::MaximumMatches`. Files are decoded incrementally in bounded
chunks instead of being copied into one byte buffer and one full `QString`.
Searches are cancellable during traversal and decoding, and only the newest
search reports.

### `editor/BackgroundWorkScheduler`

Provides process-wide, bounded pools for interactive computation, disk IO, and
low-priority background maintenance. Services borrow these pools instead of
creating an unbounded collection of private threads or implicitly competing on
Qt's global pool. This keeps foreground search and semantic work responsive
while bounding save, reload, preview, and file-filter concurrency.

### `editor/ProjectPathFilter`

Centralizes directory exclusions for project traversal. Standard generated and
metadata directories are skipped consistently, root `.gitignore` rules retain
their order and negation semantics, and callers may add temporary glob rules
without changing repository files.

### `versioncontrol/GitRepositoryService`

Provides the UI-independent Git foundation. It locates and version-checks the
system Git executable, discovers the repository containing a directory, and
coalesces explicit status refresh requests. `GitCommandRunner` launches Git
asynchronously with separate arguments, C-locale output, disabled terminal
prompts, and no optional index locks for read-only work. `GitStatusParser`
turns NUL-delimited porcelain-v2 output into immutable repository snapshots,
including branch/upstream counts, stash count, renames, conflicts, and unusual
file names. The service performs no index or working-tree mutations; UI and
future operation coordinators consume it rather than parsing Git output directly.
`MainWindow` binds the current explorer root to this service. Repository
snapshots populate a clickable branch summary in the status bar and lightweight
right-aligned status markers in the existing file-tree delegate. Source-model,
editor-save, external-change, and window-activation notifications request a
debounced refresh; non-repositories and unavailable Git installations leave the
ordinary editor UI unchanged.

### `editor/EditorPane`

Pure view of one editor pane: the tab stack of document pages, the file combo box, close/split-view buttons, and the Ctrl+Tab switch popup. It exposes a small page-container API and view-level signals (`pageCloseRequested`, `currentPageChanged`, `splitViewToggled`) and contains no file IO or document-state policy.

### `editor/DocumentController`

Drives the document lifecycle of one pane: open/save/save-as/close workflows with save prompts, the default blank document policy, shared documents for split view, applying externally reloaded content, encoding overrides (reopen/convert), and file-watcher registration. It owns `EditorManager` and a per-controller `DocumentPersistenceService`; save ordering and file watching are injected application-scoped services.

Content above a per-tick budget is inserted in chunks (`insertContentInChunks`) so a large file cannot freeze the UI mid-load. While a synchronous save drain is processing events, `m_drainingSaves` rejects re-entrant opens, closes, reloads and encoding changes.

### `editor/DocumentSession`

Shared metadata for views backed by the same Scintilla document. It carries the detected encoding/EOL format, persistence state, and external-conflict state so split panes cannot disagree about document safety.

### `editor/DocumentSaveCoordinator`

Application-scoped FIFO and generation tracker. It serializes writes per normalized destination, coalesces obsolete snapshots, and replaces process-lifetime static state.

### `editor/DocumentPath` and `editor/DocumentFormat`

`DocumentPath` is the single file-identity policy, including canonical-parent handling for files that do not exist yet and Windows case folding. `DocumentFormat` strictly validates UTF-8/BOM-marked UTF-16 and preserves uniform line endings; mixed endings are normalized to LF on save. `decodeWithEncoding()` bypasses detection for the user-chosen encoding behind Tools ▸ Encoding.

### `editor/EditorManager` and `editor/SemanticHighlightingEngine`

Creates and configures `CustomEditor` instances, applies fonts, asks the
exported language registry for syntax lexers, attaches the semantic engine, and
owns application-only editor decorators. `SemanticHighlightingEngine` owns the
viewport snapshot, debounce, cancellation, worker dispatch, and guarded result
application. Reusable editing and rendering mechanics live in the
NotePeekScintilla fork.

Semantic refresh is a visible-range incremental service: the UI thread copies
only 40 lines behind and 80 lines ahead of the viewport, then a bounded
interactive worker analyzes that snapshot with cancellation and a 2 ms budget.
Edits are debounced for 120 ms, completed navigation for 45 ms, hidden
editors schedule no work, and stale revisions never reach the widget. Results
replace only their analyzed line range and the token cache is capped at 12,000
entries. Dynamic diagnostic properties on each editor expose snapshot size,
worker/apply time, in-flight state and request/cancellation counts to tests.

The search/bookmark/current-line scrollbar markers are a mouse-transparent
overlay on QScintilla's original scrollbar. The overlay preserves the existing
document-position markers while leaving scrolling and thumb dragging on the
native QScintilla connection; replacing the scrollbar is intentionally avoided
because it forces a much slower repaint path on macOS.

### `editor/DocumentPersistenceService`

Wraps document load/save operations and save-file naming policy. UI classes can present errors, but file IO policy lives here.

### `editor/DocumentRegistry`

Provides open-document queries using the shared `DocumentPath` identity policy.

### `editor/DocumentPage`

Adapts a `CustomEditor` into either a plain editor page or a Markdown-capable page with preview support.

### `editor/SplitEditorCoordinator`

Synchronizes the pane `DocumentController`s (active pane tracking, mirroring opened/closed/renamed files, external reloads) and keeps split-view behavior out of `MainWindow`. It operates on an ordered controller list rather than a fixed left/right pair, so pane count is not baked into the logic; the two-pane constructor is a convenience wrapper.

Disk reads for externally changed files happen here, off the UI thread; controllers only apply already-loaded content. One read per path is in flight at a time and further change events coalesce into a single re-run.

### `QScintilla/NotePeek/LexerFactory` and custom lexers

The NotePeek QScintilla fork owns `LexerFactory`, maps languages, extensions,
and file names to QScintilla lexers through two declarative tables, and ships
the Dockerfile, qmake, Starlark, Meson, Groovy, and ignore-file lexers. MainPage
consumes the exported registry through `<NotePeek/lexerfactory.h>`.

### `QScintilla/NotePeek` editor kit

`NotePeek::Editor` exports bracket assistance, folding, document completion and
an LSP-compatible UTF-16 semantic-token overlay. `SyntaxStyler` translates
lexer-specific styles into 22 stable roles. `SemanticTokenProvider` is the
replaceable language-service boundary; the bundled lightweight provider gives
immediate function/type/member distinctions while allowing a future clangd or
other LSP client to provide more precise tokens without changing the widget.
`SemanticDocument`, `SemanticAnalysisControl`, and `SemanticAnalysisResult`
carry revision/range metadata, cancellation and execution budgets so providers
can be hosted asynchronously without coupling worker threads to QWidget state.

`SyntaxThemeCatalog` validates JSON themes and merges ten embedded defaults with
optional `*.json` files beside the executable and in the per-user theme folder.
External definitions can override an earlier theme with the same id and
appearance. MainPage persists separate dark and light syntax theme choices;
the active one is always selected by the current system appearance, so syntax
highlighting tracks the light/dark theme automatically. The settings UI shows
both choices at all times.

### `themecolors.h`

Application chrome colours live in one data table (`roleColors()`) keyed by
`ColorRole`. Syntax-editor colours come from the selected JSON theme in
NotePeekScintilla; `themecolors.h` remains the automatic-theme fallback and the
source of non-editor widget colours.

## Dependency Direction

Application dependencies should flow inward:

```text
MainWindow/widgets -> editor UI -> editor services -> pure Qt/core helpers
```

Avoid making low-level editor services depend on `MainWindow` or application menus. When new features need cross-widget coordination, prefer a small coordinator object over adding more responsibilities to `MainWindow`, `DocumentController`, or `EditorPane`.

## Third-Party Code

Third-party code is intentionally isolated:

- `src/QScintilla`
- `src/third_party/cmark-gfm`
- `src/third_party/qlitehtml`

The QScintilla directory is an intentional GPLv3 NotePeek fork. Its upstream
identity and local changes are documented in `UPSTREAM.md` and `PATCHES.md`.
Changes to the other third-party directories should be minimized and documented
in the commit or pull request.
