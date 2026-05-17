#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"

files=(
    "$PREFIX/bin/notepeek"
    "$PREFIX/lib/libqscintilla2.so"
    "$PREFIX/lib/libqscintilla2.so.15"
    "$PREFIX/lib/libqscintilla2.so.0.1.0"
    "$PREFIX/share/applications/notepeek.desktop"
    "$PREFIX/share/metainfo/notepeek.appdata.xml"
    "$PREFIX/share/icons/hicolor/scalable/apps/notepeek.svg"
)

for file in "${files[@]}"; do
    if [ -e "$file" ] || [ -L "$file" ]; then
        rm -f "$file"
        echo "Removed $file"
    fi
done

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f "$PREFIX/share/icons/hicolor" >/dev/null 2>&1 || true
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$PREFIX/share/applications" >/dev/null 2>&1 || true
fi

echo "NotePeek removed from $PREFIX"
