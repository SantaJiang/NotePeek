# NotePeek QScintilla Fork

This directory contains the NotePeek-maintained QScintilla fork.

- Upstream project: QScintilla by Riverbank Computing
- Upstream release: 2.14.1
- Fork release: 1.0.0
- Fork binary name: `NotePeekScintilla`
- Initial fork baseline: `QScintilla_src-2.14.1`
- License: GNU GPL version 3.0; see `LICENSE`

The upstream version macros remain unchanged so existing QScintilla source
checks continue to describe API compatibility.  The
`NOTEPEEK_QSCINTILLA_VERSION*` macros identify the fork itself.

New NotePeek-facing APIs should be kept in the `NotePeek` namespace and in
dedicated source files where possible. They are compiled by the application
static `notepeek_editor_kit` target and must not be added to the exported fork
binary. Changes to the Scintilla engine and upstream QScintilla wrappers must
be listed in `PATCHES.md` so a future upstream refresh can replay or retire them
deliberately.
