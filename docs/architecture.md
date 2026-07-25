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

`SearchResultsDock` is the project-wide search UI: query row, status line, and a file-grouped result tree that emits `matchActivated` for jump-to-match. `FileSearcher` walks the directory tree and matches text on a worker thread, filtering candidates through `SupportedFilePolicy` and capping results at `FileSearcher::MaximumMatches`. Searches are cancellable and only the newest search reports.

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

### `editor/EditorManager`

Creates and configures `CustomEditor` instances, applies fonts, syntax lexers, theme colors, and editor decorators.

### `editor/DocumentPersistenceService`

Wraps document load/save operations and save-file naming policy. UI classes can present errors, but file IO policy lives here.

### `editor/DocumentRegistry`

Provides open-document queries using the shared `DocumentPath` identity policy.

### `editor/DocumentPage`

Adapts a `CustomEditor` into either a plain editor page or a Markdown-capable page with preview support.

### `editor/SplitEditorCoordinator`

Synchronizes the pane `DocumentController`s (active pane tracking, mirroring opened/closed/renamed files, external reloads) and keeps split-view behavior out of `MainWindow`. It operates on an ordered controller list rather than a fixed left/right pair, so pane count is not baked into the logic; the two-pane constructor is a convenience wrapper.

Disk reads for externally changed files happen here, off the UI thread; controllers only apply already-loaded content. One read per path is in flight at a time and further change events coalesce into a single re-run.

### `editor/LexerFactory` and `editor/lexers/`

`LexerFactory` maps languages, extensions, and file names to QScintilla lexers through two declarative tables; adding a language is a one-line table entry. Custom lexers (Dockerfile, qmake, Starlark, ...) live one-per-file under `editor/lexers/`.

### `themecolors.h`

All themable colours live in one data table (`roleColors()`) keyed by `ColorRole`, with a light and a dark value per role. The per-widget namespaces (`Editor`, `Lexer`, `FileTree`, ...) are thin accessors over it, so a future user-selectable theme only needs to replace the table lookup rather than the call sites. Theme selection currently follows the OS colour scheme.

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

Local changes to third-party code should be minimized and documented in the commit or pull request.
