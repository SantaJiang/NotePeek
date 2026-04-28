# NotePeek

A modern cross-platform code/text editor powered by Qt (5.15+ or 6.2+) and QScintilla.

## Build

Requirements: Qt 5.15+ / Qt 6.2+, CMake 3.16+, C++17 compiler.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build --config Release --parallel
```

## Package

```bash
cd build

# macOS: DMG
cpack -G DragNDrop

# Windows: NSIS installer
cpack -G NSIS

# Linux: DEB + RPM
cpack -G "DEB;RPM"
```

## Linux Direct Install

```bash
chmod +x deploy/linux/install.sh
sudo ./deploy/linux/install.sh

chmod +x deploy/linux/uninstall.sh
sudo ./deploy/linux/uninstall.sh
```
