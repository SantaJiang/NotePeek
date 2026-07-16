#!/usr/bin/env bash
set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"

files=(
    "$PREFIX/bin/notepeek"
    "$PREFIX/share/applications/notepeek.desktop"
    "$PREFIX/share/metainfo/notepeek.appdata.xml"
    "$PREFIX/share/icons/hicolor/scalable/apps/notepeek.svg"
    "$PREFIX/share/notepeek/THIRD_PARTY_NOTICES.md"
)

for file in "${files[@]}"; do
    if [ -e "$file" ] || [ -L "$file" ]; then
        rm -f "$file"
        echo "Removed $file"
    fi
done

for library_dir in "$PREFIX/lib/notepeek" "$PREFIX/lib64/notepeek"; do
    if [ -d "$library_dir" ]; then
        while IFS= read -r -d '' file; do
            rm -f "$file"
            echo "Removed $file"
        done < <(find "$library_dir" -maxdepth 1 \( -type f -o -type l \) \
            -name 'libqscintilla2.so*' -print0)
    fi
done

rmdir "$PREFIX/lib/notepeek" "$PREFIX/lib64/notepeek" \
    "$PREFIX/share/notepeek" 2>/dev/null || true

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f "$PREFIX/share/icons/hicolor" >/dev/null 2>&1 || true
fi

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$PREFIX/share/applications" >/dev/null 2>&1 || true
fi

echo "NotePeek removed from $PREFIX"
