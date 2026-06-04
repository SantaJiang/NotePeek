# NotePeek

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platforms: macOS | Windows | Linux](https://img.shields.io/badge/Platforms-macOS%20%7C%20Windows%20%7C%20Linux-lightgrey)](https://github.com/SantaSpeeds/NotePeek/releases)
[![Qt](https://img.shields.io/badge/Qt-5.15%2B%20%7C%206.2%2B-green)](https://www.qt.io/)

**NotePeek** is a modern, lightweight, cross-platform code/text editor built with Qt and QScintilla. It provides a clean interface, essential editing features, and native packaging for macOS, Windows, and Linux.

## ✨ Features

### 📝 Core Editing
- **Multi-tab editing** – Open multiple files in tabs, with drag‑and‑drop reordering
- **Syntax highlighting** – Supports **50+ programming/markup/configuration languages and file formats** (see list below)
- **Find & Replace** – Full‑featured search with scroll‑bar result markers
- **C/C++ header‑source switching** – Quick jump between `.h` and `.cpp/.cxx` files
- **File‑system browser** – Integrated directory tree (dock) for quick file opening

### 🎨 Markdown & Preview
- **Live Markdown preview** – Real‑time rendering of `.md` files using the embedded md4c parser
- **Side‑by‑side view** – Edit Markdown on the left, see formatted output on the right

### 🌍 Cross‑Platform
- **Native look & feel** – Fusion style on macOS/Linux, Windows Vista style on Windows
- **High‑DPI support** – Automatically adapts to Retina and Hi‑DPI displays
- **System integration** – macOS: double‑click files in Finder; Windows: Start‑menu shortcut; Linux: desktop entry & AppStream metadata
- **Internationalization** – Built‑in translation system (Chinese included)

### 📦 Packaging & Deployment
- **Single‑click installers** – DMG for macOS, NSIS for Windows, DEB/RPM for Linux
- **Bundled dependencies** – QScintilla and md4c are embedded, no external runtime required
- **Direct‑install script** – Quick `/usr/local` installation on Linux without a package manager

## 📋 Supported Languages

NotePeek’s syntax highlighting covers **50+ languages and file formats** via integrated QScintilla lexers plus lightweight built-in lexers for common project files:

| Category | Languages |
|----------|-----------|
| **General‑purpose** | C, C++, C#, D, Java, Python, Rust, Ruby, Pascal, Fortran, Fortran77, Octave, MATLAB, Lua, Perl, Tcl, Verilog, VHDL, IDL, SPICE, POV‑Ray |
| **Web & markup** | HTML, XML, QML, JSON, JSONL, YAML, TOML, Markdown, TeX, CSS, Properties, PO |
| **Scripting** | Shell (Bash), Batch, CoffeeScript, Groovy/Jenkinsfile, AVS, Diff |
| **Build systems** | CMake, Makefile, Dockerfile/Containerfile, Bazel/Starlark, Meson, qmake |
| **Miscellaneous** | SQL, PostScript, ignore files (`.gitignore`, `.dockerignore`, etc.) |

## 🖥️ Supported Platforms

| Platform | Package Format | Minimum System |
|----------|----------------|----------------|
| **macOS** | `.dmg` (Drag‑and‑Drop disk image) | macOS 13.0+ |
| **Windows** | NSIS installer (`.exe`) | Windows 10/11 (64‑bit) |
| **Linux** | DEB (Debian/Ubuntu) & RPM (Fedora/openSUSE) | glibc 2.28+ |

## 🚀 Getting Started

### Prerequisites

- **Qt** 5.15+ or Qt 6.2+ (with components: Widgets, PrintSupport, Svg, LinguistTools)
- **CMake** 3.16+
- **C++17** compiler (GCC/Clang/MSVC)
- **Optional for packaging**:
  - macOS: `hdiutil` (built‑in)
  - Windows: [NSIS](https://nsis.sourceforge.io/)
  - Linux: `dpkg‑deb` & `rpmbuild` (for DEB/RPM)

### Build from Source

```bash
# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Compile
cmake --build build --config Release --parallel
```

The executable will be placed in `build/src/MainPage/` (or `build/src/MainPage/Release` on Windows).

### Package for Distribution

```bash
# macOS → DMG
cpack --config build/CPackConfig.cmake -G DragNDrop

# Windows → NSIS installer (requires NSIS installed)
cpack --config build/CPackConfig.cmake -G NSIS

# Linux → DEB + RPM
cpack --config build/CPackConfig.cmake -G "DEB;RPM"
```

### Direct Install on Linux (without packages)

```bash
# Install to /usr/local
sudo env PREFIX=/usr/local ./deploy/linux/install.sh

# Uninstall
sudo env PREFIX=/usr/local ./deploy/linux/uninstall.sh
```

## 🏗️ Project Structure

```
NotePeek/
├── CMakeLists.txt              # Root CMake + CPack configuration
├── LICENSE                     # GPL‑3.0‑or‑later
├── README.md                   # This file
├── i18n/                       # Translation files (notepeek_zh_CN.ts)
├── deploy/                     # Platform‑specific deployment assets
│   ├── mac/Info.plist          # macOS Bundle plist
│   ├── win/app.rc.in           # Windows resource template
│   ├── win/notepeek.ico        # Windows icon
│   └── linux/                  # Linux desktop integration
│       ├── notepeek.desktop    # Desktop entry
│       ├── notepeek.appdata.xml# AppStream metadata
│       ├── install.sh          # Direct‑install script
│       └── uninstall.sh        # Uninstall script
└── src/                        # Source code
    ├── CMakeLists.txt          # Sub‑directory aggregation
    ├── QScintilla/             # Embedded QScintilla library
    ├── third_party/md4c/       # Embedded md4c Markdown parser
    └── MainPage/               # Main application
        ├── CMakeLists.txt      # Build, link, package, deploy
        ├── main.cpp            # Entry point (high‑DPI, style, translation, font)
        ├── mainwindow.h/.cpp/.ui  # Main window
        ├── editor/             # Editor core (CustomEditor, MultiEditor, LexerFactory…)
        ├── find/               # Find‑replace manager & widget
        ├── decorators/         # Scroll‑bar result markers
        ├── widgets/            # Status‑bar components
        ├── tool/cpp/           # C/C++ header‑source switching
        ├── tool/markdown/      # Markdown previewer
        ├── resource/           # Icons (.icns, .svg)
        └── rec.qrc             # Qt resource file
```

## 📄 License

NotePeek is licensed under the **GNU General Public License v3.0 (or later)** – see the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing‑feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing‑feature`)
5. Open a Pull Request

## 📬 Contact

- **GitHub**: [SantaSpeeds/NotePeek](https://github.com/SantaSpeeds/NotePeek)
- **Issues**: [GitHub Issues](https://github.com/SantaSpeeds/NotePeek/issues)

---

*NotePeek – a peek at your notes, code, and text, everywhere.*
