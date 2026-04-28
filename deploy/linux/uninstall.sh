#!/bin/bash

echo "正在卸载NotePeek..."

# 1. 删除图标文件
DEST_ICON="/usr/share/icons/hicolor/scalable/apps/notepeek.svg"
if [ -f "$DEST_ICON" ]; then
    echo "删除图标文件: $DEST_ICON"
    sudo rm -f "$DEST_ICON" && echo "✓ 图标文件删除成功"
else
    echo "⚠ 未找到图标文件: $DEST_ICON"
fi

# 2. 删除可执行文件
DEST_BIN="/usr/bin/notepeek"
if [ -f "$DEST_BIN" ]; then
    echo "删除可执行文件: $DEST_BIN"
    sudo rm -f "$DEST_BIN" && echo "✓ 可执行文件删除成功"
else
    echo "⚠ 未找到可执行文件: $DEST_BIN"
fi

# 3. 删除库文件
# 查找并删除所有libqscintilla2.so相关的库文件
echo "正在查找并删除库文件..."
LIB_FILES=$(ls /usr/lib/libqscintilla2.so* 2>/dev/null)

if [ -n "$LIB_FILES" ]; then
    echo "找到以下库文件:"
    for lib in $LIB_FILES; do
        echo "  - $lib"
    done
    
    read -p "是否删除这些库文件? [y/N]: " confirm
    if [[ $confirm =~ ^[Yy]$ ]]; then
        sudo rm -f /usr/lib/libqscintilla2.so*
        echo "✓ 库文件删除成功"
    else
        echo "✗ 用户取消删除库文件"
    fi
else
    echo "⚠ 未找到libqscintilla2.so库文件"
fi

# 4. 删除桌面文件
DEST_DESKTOP="/usr/share/applications/notepeek.desktop"
if [ -f "$DEST_DESKTOP" ]; then
    echo "删除桌面文件: $DEST_DESKTOP"
    sudo rm -f "$DEST_DESKTOP" && echo "✓ 桌面文件删除成功"
else
    echo "⚠ 未找到桌面文件: $DEST_DESKTOP"
fi

# 5. 删除翻译文件
DEST_TRANSLATION="/usr/bin/notepeek_zh_CN.qm"
if [ -f "$DEST_TRANSLATION" ]; then
    echo "删除翻译文件: $DEST_TRANSLATION"
    sudo rm -f "$DEST_TRANSLATION" && echo "✓ 翻译文件删除成功"
else
    echo "⚠ 未找到翻译文件: $DEST_TRANSLATION"
fi

# 6. 删除应用程序缓存
echo "清理应用程序缓存..."
# 更新图标缓存
if [ -x "$(command -v gtk-update-icon-cache)" ]; then
    sudo gtk-update-icon-cache -f /usr/share/icons/hicolor/
    echo "✓ 图标缓存已更新"
fi

# 更新桌面数据库
if [ -x "$(command -v update-desktop-database)" ]; then
    sudo update-desktop-database /usr/share/applications/
    echo "✓ 桌面数据库已更新"
fi

echo ""
echo "========================================"
echo "🗑️  NotePeek 卸载完成！"
echo "========================================"
echo "以下文件已被删除或确认不存在:"
echo "  • /usr/share/icons/hicolor/scalable/apps/notepeek.svg"
echo "  • /usr/bin/notepeek"
echo "  • /usr/lib/libqscintilla2.so*"
echo "  • /usr/share/applications/notepeek.desktop"
echo "  • /usr/bin/notepeek_zh_CN.qm"
echo ""
echo "注意: 用户配置文件和缓存目录未被删除:"
echo "  • ~/.config/NotePeek/ (如果存在)"
echo "  • ~/.cache/NotePeek/ (如果存在)"
