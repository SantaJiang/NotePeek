# Third-Party Notices

NotePeek vendors the following source dependencies so releases can be built reproducibly.
Their original license texts remain in the referenced source directories.

| Dependency | Bundled version | Purpose | License location |
|---|---:|---|---|
| QScintilla | 2.14.1 | Text editing component | `src/QScintilla/Qsci/qsciglobal.h` and Scintilla `License.txt` files |
| Scintilla | Bundled with QScintilla | Editing engine and lexers | `src/QScintilla/scintilla/**/License.txt` |
| cmark-gfm | Vendored snapshot | GitHub-Flavored Markdown parsing | `src/third_party/cmark-gfm/COPYING` |
| qlitehtml | Vendored snapshot | Qt HTML preview widget | `src/third_party/qlitehtml/LICENSES/` |
| litehtml | Vendored with qlitehtml | HTML layout engine | `src/third_party/qlitehtml/3rdparty/litehtml/LICENSE` |
| Gumbo parser | Vendored with litehtml | HTML parsing | `src/third_party/qlitehtml/3rdparty/litehtml/src/gumbo/LICENSE` |

When updating a vendored dependency, update its version above, retain upstream
license files, and run the full build, test, performance, and install checks on
macOS, Windows, and Linux.
