#!/bin/bash

# 获取脚本当前所在目录的绝对路径
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "正在安装NotePeek..."
echo "脚本路径: $SCRIPT_DIR"

# 1. 复制图标文件
SRC_ICON="$SCRIPT_DIR/src/MainPage/resource/notepeek.svg"
DEST_ICON_DIR="/usr/share/icons/hicolor/scalable/apps/"
if [ -f "$SRC_ICON" ]; then
    echo "复制图标文件: $SRC_ICON -> $DEST_ICON_DIR"
    sudo cp "$SRC_ICON" "$DEST_ICON_DIR" && echo "✓ 图标文件复制成功"
else
    echo "✗ 错误: 找不到图标文件 $SRC_ICON"
    exit 1
fi

# 2. 复制可执行文件
SRC_BIN="$SCRIPT_DIR/src/Bin/Release/notepeek"
DEST_BIN_DIR="/usr/bin/"
if [ -f "$SRC_BIN" ]; then
    echo "复制可执行文件: $SRC_BIN -> $DEST_BIN_DIR"
    sudo cp "$SRC_BIN" "$DEST_BIN_DIR" && echo "✓ 可执行文件复制成功"
    # 确保可执行权限
    sudo chmod +x "$DEST_BIN_DIR/notepeek"
else
    echo "✗ 错误: 找不到可执行文件 $SRC_BIN"
    exit 1
fi

# 3. 复制库文件（使用通配符匹配所有相关库文件）
SRC_LIB="$SCRIPT_DIR/src/Bin/Release/libqscintilla2.so*"
DEST_LIB_DIR="/usr/lib/"
if ls $SRC_LIB 1> /dev/null 2>&1; then
    echo "复制库文件: $SRC_LIB -> $DEST_LIB_DIR"
    sudo cp $SRC_LIB "$DEST_LIB_DIR" && echo "✓ 库文件复制成功"
else
    echo "✗ 错误: 找不到库文件 $SRC_LIB"
    exit 1
fi

# 4. 复制桌面文件
SRC_DESKTOP="$SCRIPT_DIR/deploy/linux/notepeek.desktop"
DEST_DESKTOP_DIR="/usr/share/applications/"
if [ -f "$SRC_DESKTOP" ]; then
    echo "复制桌面文件: $SRC_DESKTOP -> $DEST_DESKTOP_DIR"
    sudo cp "$SRC_DESKTOP" "$DEST_DESKTOP_DIR" && echo "✓ 桌面文件复制成功"
else
    echo "✗ 错误: 找不到桌面文件 $SRC_DESKTOP"
    exit 1
fi

# 5. 复制翻译文件
SRC_TRANSLATION="$SCRIPT_DIR/i18n/notepeek_zh_CN.qm"
DEST_TRANSLATION_DIR="/usr/bin/"
if [ -f "$SRC_TRANSLATION" ]; then
    echo "复制翻译文件: $SRC_TRANSLATION -> $DEST_TRANSLATION_DIR"
    sudo cp "$SRC_TRANSLATION" "$DEST_TRANSLATION_DIR" && echo "✓ 翻译文件复制成功"
else
    echo "✗ 错误: 找不到翻译文件 $SRC_TRANSLATION"
    exit 1
fi

echo ""
echo "========================================"
echo "✅ NotePeek 安装完成！"
echo "========================================"
echo "安装的项目:"
echo "  • 图标文件: /usr/share/icons/hicolor/scalable/apps/notepeek.svg"
echo "  • 可执行文件: /usr/bin/notepeek"
echo "  • 库文件: /usr/lib/libqscintilla2.so*"
echo "  • 桌面文件: /usr/share/applications/notepeek.desktop"
echo "  • 翻译文件: /usr/bin/notepeek_zh_CN.qm"
echo ""
echo "您可以在应用程序菜单中找到NotePeek，或直接运行命令: notepeek"
