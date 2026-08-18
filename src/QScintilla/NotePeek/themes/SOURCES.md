# Syntax theme sources

NotePeek ships ten semantic-role adaptations of upstream editor themes. Colors
come from the pinned upstream files below; the visible names omit editor and
vendor names. Every JSON file also keeps its repository, revision, and license
in the `source` object so provenance travels with the theme.

The adaptation maps TextMate or Qt Creator style categories onto NotePeek's 22
cross-language syntax roles. Upstream font-style settings are not represented
because NotePeek's theme model currently stores colors only.

## Visual Studio Code defaults

- Repository: <https://github.com/microsoft/vscode>
- Revision: `7fee81510403f95dff38c5f4a528dbc4bd4d4298`
- License: MIT (`LICENSE.txt` in the upstream repository)
- Adapted files:
  - `extensions/theme-defaults/themes/2026-dark.json` as `Dark 2026`
  - `extensions/theme-defaults/themes/dark_plus.json` as `Dark+`
  - `extensions/theme-defaults/themes/2026-light.json` as `Light 2026`
  - `extensions/theme-defaults/themes/light_plus.json` as `Light+`
  - `extensions/theme-quietlight/themes/quietlight-color-theme.json` as `Quiet Light`

## Qt Creator

- Repository: <https://github.com/qt-creator/qt-creator>
- Revision: `266254593f77ccb252fe145908d2d66bf14d52de`
- License: GPL-3.0-only WITH Qt-GPL-exception-1.0
- Adapted file: `share/qtcreator/styles/dark-2024.xml` as `Dark 2024`

## Rosé Pine

- Repository: <https://github.com/rose-pine/vscode>
- Revision: `d8f5ebe8e096fa833e997c07eb7685ee1677a4ba`
- License: MIT
- Adapted themes: `Rosé Pine` and `Rosé Pine Dawn`

## Ayu

- Repository: <https://github.com/ayu-theme/vscode-ayu>
- Revision: `444ef92911cb75c3933c8003e3a7c79b6b6c914f`
- License: MIT
- Adapted themes: `Ayu Dark` and `Ayu Light`

## External theme files

At startup NotePeek loads additional `*.json` files from both the `themes`
directory beside the application executable and the per-user theme directory
shown by Preferences. The user directory is loaded last. An external theme
with the same `id` therefore replaces the earlier built-in or installation
copy without modifying the application package.

External files use the same schema as the built-ins and must define `id`,
`name`, `appearance`, source provenance, all required editor colors, and all 22
token colors. Invalid files are ignored. Preferences can validate and import a
JSON file into the per-user directory.

## MIT notice

Copyright (c) 2015-present Microsoft Corporation

Copyright (c) 2021 Rosé Pine

Copyright (c) 2016 Ike Kurghinyan

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
