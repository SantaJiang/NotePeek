# Third-Party Notices

NotePeek vendors the following source dependencies so releases can be built reproducibly.
Their original license texts remain in the referenced source directories.

| Dependency | Bundled version | Upstream | Purpose | License location |
|---|---:|---|---|---|
| QScintilla | 2.14.1 | `riverbankcomputing.com/software/qscintilla` | Text editing component | `src/QScintilla/Qsci/qsciglobal.h` and Scintilla `License.txt` files |
| Scintilla | Bundled with QScintilla 2.14.1 | `scintilla.org` | Editing engine and lexers | `src/QScintilla/scintilla/**/License.txt` |
| cmark-gfm | 0.29.0.gfm.13 | `github.com/github/cmark-gfm` | GitHub-Flavored Markdown parsing | `src/third_party/cmark-gfm/COPYING` |
| qlitehtml | Qt vendored snapshot | `code.qt.io/cgit/qt/qlitehtml.git` | Qt HTML preview widget | `src/third_party/qlitehtml/LICENSES/` |
| litehtml | Vendored snapshot | `github.com/litehtml/litehtml` | HTML layout engine | `src/third_party/qlitehtml/3rdparty/litehtml/LICENSE` |
| Gumbo parser | 0.10.1 | `github.com/google/gumbo-parser` | HTML parsing | `src/third_party/qlitehtml/3rdparty/litehtml/src/gumbo/LICENSE` |

When updating a vendored dependency, update its version above, retain upstream
license files, and run the full build, test, performance, and install checks on
macOS, Windows, and Linux.

The update and warning-baseline procedure is documented in
`docs/dependency-policy.md`. Compiler warnings emitted by vendored targets are
tracked separately from NotePeek warnings; `NOTEPEEK_WARNINGS_AS_ERRORS` still
applies to every NotePeek-owned target.
