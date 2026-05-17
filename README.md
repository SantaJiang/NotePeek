# NotePeek

A modern cross-platform code/text editor powered by Qt (5.15+ or 6.2+) and QScintilla.

## Features

- Multi-tab text/code editing
- Syntax highlighting via bundled QScintilla
- Find/replace with scrollbar result markers
- Markdown preview for `.md` files
- Header/source switching for C/C++ files
- Native packaging for macOS, Windows, and Linux

## Build

Requirements:

- Qt 5.15+ or Qt 6.2+ with Widgets, PrintSupport, Svg, Network, and LinguistTools
- CMake 3.16+
- C++17 compiler
- NSIS for Windows installer packaging
- `rpmbuild` for RPM packaging on Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Release --parallel
```

## Package

```bash
# macOS: DMG
cpack --config build/CPackConfig.cmake -G DragNDrop

# Windows: NSIS installer
cpack --config build/CPackConfig.cmake -G NSIS

# Linux: DEB + RPM
cpack --config build/CPackConfig.cmake -G "DEB;RPM"
```

## Linux Direct Install

The direct install scripts wrap CMake install and use `/usr/local` by default.

```bash
sudo env PREFIX=/usr/local ./deploy/linux/install.sh

sudo env PREFIX=/usr/local ./deploy/linux/uninstall.sh
```

For normal distribution, prefer the generated DEB/RPM packages.

## Notes

- Translations are embedded into the application resource system and loaded from `:/i18n`.
- The bundled QScintilla library is installed inside the macOS app bundle for DMG packaging.
- On Linux, CPack packages install the executable, bundled QScintilla library, desktop entry, AppStream metadata, and icon.
