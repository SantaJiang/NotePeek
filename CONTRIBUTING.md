# Contributing

Thanks for taking the time to improve NotePeek.

## Development Setup

Install Qt 5.15+ or Qt 6.2+, CMake 3.16+, and a C++17 compiler. Then build from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

## Code Style

The repository uses `.editorconfig` and `.clang-format` to keep diffs consistent. Prefer focused changes that keep UI code thin and move reusable logic into small classes under the relevant module.

## Architecture Guidelines

- Keep third-party code under `src/third_party` or `src/QScintilla`.
- Keep application code under `src/MainPage`.
- Prefer service/controller classes for file IO, document state, and coordination logic.
- Keep `MainWindow` responsible for top-level composition only.
- Keep widget classes responsible for presentation and user interaction, not broad application policy.

## Pull Requests

Before opening a pull request, build the project locally and describe the user-visible behavior that changed. For refactors, call out the module boundaries that were changed and why.
