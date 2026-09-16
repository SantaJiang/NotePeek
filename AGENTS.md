# AGENTS.md — Constraints for AI-assisted changes

This file gates how AI coding agents (and their human reviewers) operate on this
repository. The three rules below are **mandatory**; run them in order after
every code change. `CONTRIBUTING.md` is the human-facing companion; this file
adds the guardrails an AI agent must self-enforce.

## 1. Verify CI compatibility after every change

GitHub Actions only runs the packaging matrix when `scripts/ci_version_gate.py`
says so — and that happens **only on a minor (middle) version bump or an
unpublished-release retry**. Patch-only version bumps, edits to CI
infrastructure files, and pushes whose history cannot be compared are skipped
to preserve GitHub Actions quota; trigger the workflow manually when needed.
Automated GitHub jobs compile and package with `BUILD_TESTING=OFF`; all tests
must pass locally before pushing. A minor version bump still triggers every
packaging job below, so treat a one-line version edit as a multi-platform build
event, not a trivial safe change.

**Packaging matrix** (`build-and-package`): Ubuntu x86_64, Ubuntu arm64,
Windows, macOS arm64 — all on Qt 6.8.3. macOS Intel is not in the matrix.

Before declaring work done, reason about — and where possible build — the
following. Do not limit verification to the files you touched.

- **Qt 6 API baseline.** The project requires Qt 6.2 or newer and CI packages
  with Qt 6.8.3. Do not add older-major fallbacks, version-selection options, or
  compatibility-module dependencies.
- **Build beyond the files you touched.** Latent bugs in untouched files
  surface when CI compiles the whole project. After editing, build at least
  `notepeek_core` and `notepeek_widgets` and run `ctest` with Qt 6.
- **Platform-specific behavior.** Font availability and timing-based tests
  differ across the Linux/macOS/Windows runners. A test passing on your
  machine does not prove it passes on CI.
- **Performance budgets.** Local tests assert wall-clock budgets (e.g.
  "2 MB highlight < 5 s", "viewport semantic worker < 2 ms"). These are tight
  on the macOS arm64 runner. Flag any change that touches editor rendering
  (`src/QScintilla/scintilla/src/*`, `src/MainPage/editor/*`,
  `src/MainPage/decorators/*`) as a potential perf regression and run
  `notepeek_performance_test` locally.
- **Version gate.** A release-triggering change is a `VERSION` bump in
  `CMakeLists.txt` **plus** a matching `<release>` entry in
  `deploy/linux/notepeek.appdata.xml`. `Info.plist` and `deploy/win/app.rc.in`
  derive from `PROJECT_VERSION` automatically — do not hand-edit them.

Local CI-equivalent checks (run before considering the task complete):

```bash
cmake -S . -B build-qt6 -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DNOTEPEEK_WARNINGS_AS_ERRORS=ON
cmake --build build-qt6 --parallel
ctest --test-dir build-qt6 --output-on-failure
```

If you cannot run a platform locally (no Windows, Linux, etc.), **say so
explicitly** and reason about that axis instead of staying silent. Never claim
cross-platform coverage when you only ran on one OS.

## 2. Self-review the change you just made

Before staging a commit, re-read every file in your diff end to end and look
for, at minimum:

- Qt APIs newer than the documented minimum without a guarded fallback.
- Null / empty / deselection edge cases in signal handlers
  (e.g. `currentRowChanged` receiving `-1`, `item(row)` returning `nullptr`).
- Fragile coupling between widgets (e.g. sidebar row index assumed to equal a
  stacked-page index by position alone — bind them explicitly, e.g. via
  `Qt::UserRole`).
- Signal/slot wiring: `connect` order vs. initial state sync, receiver context
  lifetime, whether a manual `setCurrentIndex` is still needed after a signal
  handler is connected.
- Unused includes or dead code left over from the edit.
- Determinism that depends on signal emission order or platform defaults;
  prefer explicit initialization over relying on a signal firing.
- Translations: after adding or changing any `tr()` string, run the
  `NotePeek_lupdate` CMake target (it feeds `qt_add_translations` with
  `-no-obsolete` across all sources) to refresh line numbers and drop obsolete
  entries, then fill in real translations. Never hand-edit stale line numbers,
  and never leave `type="unfinished"` behind.

Iterate the review until no new issue is found. Only then proceed to commit.

## 3. Clean up after AI-made commits

After a commit authored by an AI agent, leave the working tree as the human
would expect:

- Remove local build directories you created during the task: `build-qt6/`,
  `build-ci/`, `cmake-build-*/`,
  `.cache/build-*/`, and any scratch dirs under `/var/folders/…` or `/tmp`.
- Remove editor/IDE temp files you introduced (`.DS_Store`, `*.autosave`,
  a `compile_commands.json` generated outside a build dir, `.qtcreator/`
  session noise, etc.).
- Confirm with `git status` that the tree is clean — no stray tracked or
  untracked files you introduced. `git status --ignored` is useful to see what
  build output exists (it is gitignored, but should still be deleted so it
  does not pile up on disk).
- Do **not** delete or weaken `.gitignore` patterns to force-include build
  output, and do **not** commit build artifacts.
- If an earlier AI run left garbage behind, clean it as part of the current
  task rather than leaving it for the human.

---

## CI reference (quick)

- Workflow: `.github/workflows/ci.yml` (version-gated build + package +
  release). Automated Actions do not run CTest; local validation is mandatory.
- Version gate: `scripts/ci_version_gate.py` — the expensive
  `build-and-package` matrix runs only when the minor (middle) version number
  increases or an unpublished release is retried; on every other push only the
  lightweight gate job runs. `quality.yml` is `workflow_dispatch`-only (no
  schedule) and is the only GitHub workflow that runs tests.
- Translation tooling: `NotePeek_lupdate` target → regenerates
  `i18n/notepeek_zh_CN.ts`; the app build then `lrelease`s it into the bundled
  `.qm` resource.
- Sanitizers: `-DNOTEPEEK_ENABLE_SANITIZERS=ON` (NotePeek targets only; vendored
  deps keep upstream flags).
- Warnings-as-errors: `-DNOTEPEEK_WARNINGS_AS_ERRORS=ON`.
