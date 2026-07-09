# NotePeek Performance Optimization Plan

## Summary

This document records the final architecture-level performance optimization plan implemented for NotePeek. The goal is to keep the UI responsive under larger files and heavier projects by moving expensive work off the UI thread, avoiding duplicate document loads, and reducing full-document scans during routine editor updates.

## Goals

- Reduce UI-thread blocking during file open, save, Markdown preview, file-tree filtering, and external file checks.
- Avoid duplicate disk reads and duplicate memory usage when split view is enabled.
- Degrade gracefully for large files instead of applying expensive features unconditionally.
- Preserve the existing Qt 5.15+ / Qt 6.2+ dependency profile without introducing non-Qt runtime dependencies.
- Keep third-party code unchanged; optimize through application-layer services and scheduling.

## Architecture Changes

### Async Document IO

- Added Qt Concurrent as a build dependency.
- Added asynchronous load/save request and result types in `DocumentPersistenceService`.
- File open now creates a read-only page immediately and fills it when the background load completes.
- Async load/save results are guarded by request ids so stale completions cannot overwrite newer state.
- Save uses a Scintilla UTF-8 buffer snapshot instead of `editor->text()` to avoid converting the full document through `QString`.

### Safe Close And Save Semantics

- Normal Save / Save As keeps the asynchronous save path for responsiveness.
- Close/quit save prompts use synchronous save to avoid exiting before data reaches disk.
- `SplitEditorCoordinator::closeAllEditors()` waits for any already-running pending saves before checking dirty state and closing.

### Split View Memory Reduction

- Split view no longer mirrors every open file into the inactive pane.
- When split view is enabled, the other pane shares the active Scintilla document where possible.
- This avoids redundant disk reads and duplicate text buffers for the same file.

### Large File Policy

- Added shared performance thresholds in `editor/performanceconstants.h`.
- Large files disable or downgrade expensive behavior such as syntax-heavy setup, full search highlighting, Markdown auto-preview, content hashing, and full-text status calculations.
- Status bar length and selection reporting now use Scintilla-native APIs instead of copying full text.

### Markdown Preview Pipeline

- Side preview rendering now runs through an async render request/result flow.
- The UI thread snapshots theme and font style data before dispatching work.
- The worker thread performs pure Markdown parsing and HTML string construction without capturing `QObject` or touching GUI/global application state.
- Stale preview render results are discarded by generation id.
- Source-line anchors are returned as render metadata, so scroll sync no longer parses the preview HTML after every render.
- Large Markdown files show a lightweight preview-disabled message instead of repeatedly parsing the full document.

### File Tree Filtering

- File filtering now skips common heavy directories such as `.git`, `build`, `node_modules`, `DerivedData`, and Python cache directories.
- Support-probe results have a bounded cache to avoid unbounded growth in very large directory trees.
- The file tree column uses an interactive fixed sizing mode instead of `ResizeToContents`, reducing layout churn as `QFileSystemModel` populates.

### Scrollbar Marker Caching

- Search-result and bookmark ticks are cached separately.
- Paint events draw cached marker positions rather than scanning indicators or markers across the whole document.
- Search marker caches are refreshed when search results change.

### External File Watcher

- Directory and file change events are debounced through a shared timer.
- Small files still use content hashing when metadata is unchanged.
- Large files skip content hashing and rely on metadata changes to avoid expensive background IO.
- Watcher refreshes are no longer triggered on every editor modification.

## Verification

Run the standard build and test sequence:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build --output-on-failure
git diff --check
```

Additional performance fixture generation:

```bash
python3 scripts/perf_smoke.py --out build/perf-fixtures-smoke
```

The fixture script creates deterministic 1 MB, 10 MB, and 50 MB text/Markdown files plus a large directory tree for manual launch, open, save, preview, and file-tree smoke testing.

## Acceptance Scenarios

- Opening a large file should create the tab promptly while loading continues in the background.
- Saving from the menu should keep the UI responsive.
- Closing the app after saving should not exit before pending saves complete.
- Split view should show the active document without rereading every open file into the second pane.
- Markdown typing should only apply the latest preview result; older async renders must be ignored.
- Large Markdown files should not repeatedly trigger full live-preview renders.
- File-tree expansion in large projects should avoid scanning ignored heavy directories.
- Search and scrollbar painting should avoid repeated full-document marker scans.

## Follow-Up Opportunities

- Add UI-level QTests for async open cancellation, pending-save close behavior, and split-view shared documents.
- Add automated timing around the generated performance fixtures.
- Consider a user-visible large-file mode indicator when preview/highlighting features are downgraded.
- Consider streaming load for very large files if NotePeek needs to handle files beyond the current full-buffer editor model.
