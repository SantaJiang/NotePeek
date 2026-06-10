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
      -> FileExplorerDock
      -> DocumentController (left)  ──┐
      -> DocumentController (right) ──┤ each drives one EditorPane
      -> SplitEditorCoordinator     ──┘ and owns:
          (synchronizes the two        -> EditorManager
           controllers/panes)          -> DocumentPersistenceService
                                       -> ExternalFileWatcher
      -> FindReplaceManager
      -> HeaderSourceManager
      -> MarkdownPreviewer

EditorPane (view)
  -> DocumentPage (one per tab, plain or Markdown-capable)
```

## Module Responsibilities

### `MainWindow`

Owns the application shell: menus, dock placement, the find/replace widget, active editor pane routing, and top-level commands. It should not own file-system model details or document persistence details.

### `widgets/FileExplorerDock`

Owns the file-system browser dock, including the `QFileSystemModel`, file-type filter proxy, tree view behavior, root directory changes, and file selection.

### `editor/EditorPane`

Pure view of one editor pane: the tab stack of document pages, the file combo box, close/split-view buttons, and the Ctrl+Tab switch popup. It exposes a small page-container API and view-level signals (`pageCloseRequested`, `currentPageChanged`, `splitViewToggled`) and contains no file IO or document-state policy.

### `editor/DocumentController`

Drives the document lifecycle of one pane: open/save/save-as/close workflows with save prompts, the default blank document policy, shared documents for split view, reloading externally changed files, and file-watcher bookkeeping. It owns `EditorManager`, `DocumentPersistenceService`, and `ExternalFileWatcher`, and emits all document-level signals (`fileOpened`, `currentFileChanged`, `modificationChanged`, ...).

### `editor/EditorManager`

Creates and configures `CustomEditor` instances, applies fonts, syntax lexers, theme colors, and editor decorators.

### `editor/DocumentPersistenceService`

Wraps document load/save operations and save-file naming policy. UI classes can present errors, but file IO policy lives here.

### `editor/DocumentRegistry`

Provides path normalization and open-document queries used by panes and file watchers.

### `editor/DocumentPage`

Adapts a `CustomEditor` into either a plain editor page or a Markdown-capable page with preview support.

### `editor/SplitEditorCoordinator`

Synchronizes the two `DocumentController`s (active pane tracking, mirroring opened/closed/renamed files, external reloads) and keeps split-view behavior out of `MainWindow`.

### `editor/LexerFactory` and `editor/lexers/`

`LexerFactory` maps languages, extensions, and file names to QScintilla lexers through two declarative tables; adding a language is a one-line table entry. Custom lexers (Dockerfile, qmake, Starlark, ...) live one-per-file under `editor/lexers/`.

## Dependency Direction

Application dependencies should flow inward:

```text
MainWindow/widgets -> editor UI -> editor services -> pure Qt/core helpers
```

Avoid making low-level editor services depend on `MainWindow` or application menus. When new features need cross-widget coordination, prefer a small coordinator object over adding more responsibilities to `MainWindow`, `DocumentController`, or `EditorPane`.

## Third-Party Code

Third-party code is intentionally isolated:

- `src/QScintilla`
- `src/third_party/md4c`
- `src/third_party/qlitehtml`

Local changes to third-party code should be minimized and documented in the commit or pull request.
