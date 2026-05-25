# NotePeek 暗色模式适配 — 变更总结

## 背景

NotePeek 在 macOS 深色模式下，编辑器控件存在颜色显示问题：

1. **左侧行号条** — 白底黑字，在暗色界面中极为刺眼
2. **光标所在行** — 高亮背景色为浅紫色 `#E8E8FF`，在暗色背景下不协调
3. **编辑区主体** — 未做任何暗色适配，所有颜色均为硬编码的浅色方案
4. **语法高亮** — Lexer 默认颜色在暗色背景下对比度极差
5. **Markdown 预览** — CSS 全部硬编码浅色
6. **编辑器切换弹窗** — 硬编码白色背景
7. **滚动条装饰器 / 查找高亮** — 颜色未做暗色适配
8. **运行时主题切换** — 不支持，修改系统主题后需重启应用

## 架构方案

引入集中式主题颜色定义头文件 `themecolors.h`，所有组件共享同一套颜色方案，避免散落的硬编码值。

```
themecolors.h          ← 集中定义所有暗色/亮色颜色
  ├─ EditorManager     ← 编辑器控件颜色
  ├─ Lexer             ← 语法高亮颜色
  ├─ ScrollBar         ← 滚动条装饰器颜色
  ├─ FindReplace       ← 查找高亮颜色
  ├─ SwitchPopup       ← 编辑器切换弹窗颜色
  └─ MarkdownPreview   ← Markdown 预览 CSS 颜色
```

暗色模式检测方式：

| Qt 版本 | 检测方式 |
|---|---|
| Qt ≥ 6.5 | `QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark` |
| Qt < 6.5 | 回退到 `QPalette::Window` 亮度值 < 128 |

## 变更文件清单

### 新增文件

| 文件 | 说明 |
|---|---|
| `src/MainPage/themecolors.h` | 集中式主题颜色定义，包含 `isDarkMode()` 检测和 6 个命名空间的颜色 getter |

### 修改文件

| 文件 | 改动要点 |
|---|---|
| `src/MainPage/editor/editormanager.h` | 移除 `isDarkMode()` 声明和 `QColor` include；新增 `reapplyThemeToAllEditors()`、`applyLexerDarkMode()`、`reapplyEditorSyntax()` 声明 |
| `src/MainPage/editor/editormanager.cpp` | 引入 `themecolors.h`；`applyEditorColors()` 改用 `ThemeColors::Editor`，行号条背景色改用 `setMarginsBackgroundColor()`（修复 `NumberMargin` 类型下 `setMarginBackgroundColor()` 无效的问题）；新增 `applyLexerDarkMode()` 对 style 1-16 的常见语法颜色做暗色覆盖；新增 `reapplyThemeToAllEditors()` 和 `reapplyEditorSyntax()` 支持运行时主题切换 |
| `src/MainPage/editor/editorswitchpopup.h` | 新增 `applyThemeStyleSheet()` 方法声明 |
| `src/MainPage/editor/editorswitchpopup.cpp` | 引入 `themecolors.h`；构造函数中移除硬编码 stylesheet，改为调用 `applyThemeStyleSheet()`；`showPopup()` 中每次弹出前重新应用样式表以响应主题变化 |
| `src/MainPage/editor/multieditor.h` | 新增 `editorManager()` 公开访问器，供 MainWindow 调用 `reapplyThemeToAllEditors()` |
| `src/MainPage/decorators/highlightedscrollbar.h` | 移除 4 个 `const QColor` 成员（`SEARCH_RESULT_COLOR`、`BOOKMARK_COLOR`、`CURRENT_LINE_COLOR`、`SELECTION_COLOR`），尺寸常量改为 `static constexpr` |
| `src/MainPage/decorators/highlightedscrollbar.cpp` | 引入 `themecolors.h`；`drawSearchResults/drawBookmarks/drawCurrentLine` 改用 `ThemeColors::ScrollBar` 中的颜色函数 |
| `src/MainPage/decorators/highlightedscrollbardecorator.cpp` | 引入 `themecolors.h`；构造函数中指示器和标记颜色改用 `ThemeColors::ScrollBar` |
| `src/MainPage/find/findreplacemanager.cpp` | 引入 `themecolors.h`；`initializeEditor()` 中高亮指示器颜色改用 `ThemeColors::FindReplace` |
| `src/MainPage/tool/markdown/markdownpreviewer.cpp` | 引入 `themecolors.h`；`buildHtmlDocument()` 从硬编码 CSS 改为动态生成，所有颜色取自 `ThemeColors::MarkdownPreview` |
| `src/MainPage/mainwindow.h` | 新增 `changeEvent()` 重写声明 |
| `src/MainPage/mainwindow.cpp` | 引入 `themecolors.h` 和 `<QStyleHints>`；新增 `changeEvent()` 监听 `QEvent::PaletteChange`；构造函数中连接 `QStyleHints::colorSchemeChanged` 信号（Qt 6.5+），实现运行时主题实时切换 |

## 关键技术细节

### 行号条背景色修复

QScintilla 有两种设置行号条背景色的 API：

- `setMarginBackgroundColor(margin, color)` — 使用 `SCI_SETMARGINBACKN`，**仅对 `SC_MARGIN_COLOUR` 类型**的 margin 生效
- `setMarginsBackgroundColor(color)` — 使用 `STYLE_LINENUMBER` (style 33) 的 paper 属性，**对所有 margin 有效**（包括 `NumberMargin`）

项目中 margin 0 的类型是 `NumberMargin` (`SC_MARGIN_NUMBER`)，因此必须使用 `setMarginsBackgroundColor()` 才能生效。之前的实现调用了错误的 API，导致行号条始终显示为默认白色。

### Lexer 暗色适配策略

不完全替换 Lexer 的颜色方案，而是：

1. 用 `lexer->setPaper(defaultPaper, -1)` 和 `lexer->setColor(defaultColor, -1)` 覆盖默认背景/文字色
2. 对 style 1-16 的常见语法分类（注释、关键字、字符串、数字、预处理器、操作符等）设置暗色友好的颜色
3. Lexer 未使用的 style 编号会被静默忽略，不会有副作用

这种策略确保了所有语言 Lexer 都能获得暗色基底，同时保留了各 Lexer 的风格差异。

### 运行时主题切换

macOS 上使用 Fusion 风格时，`QEvent::PaletteChange` 事件不一定在系统主题切换时触发。因此采用双重监听：

- `QEvent::PaletteChange` — 兜底方案，覆盖所有 Qt 版本
- `QStyleHints::colorSchemeChanged` (Qt 6.5+) — 专用信号，在 macOS 上可靠触发

两者都调用 `EditorManager::reapplyThemeToAllEditors()`，遍历所有已打开的编辑器，重新应用颜色、Lexer 暗色覆盖，并刷新 QScintilla 内部状态。

## 暗色模式颜色方案

| 元素 | 亮色模式 | 暗色模式 |
|---|---|---|
| 行号条背景 | `#f0f0f0` | `#2d2d2d` |
| 行号条文字 | `#666666` | `#858585` |
| 光标行高亮 | `#e8e8ff` | `#2a2a2a` |
| 编辑区背景 | `#ffffff` | `#1e1e1e` |
| 编辑区文字 | `#000000` | `#d4d4d4` |
| 匹配括号前景 | 蓝色 | `#569cd6` |
| 匹配括号背景 | 黄色 | `#3b3b3b` |
| 关键字 | `#0000ff` | `#569cd6` |
| 字符串 | `#a31515` | `#ce9178` |
| 注释 | `#008000` | `#6a9955` |
| 数字 | `#000080` | `#b5cea8` |
| 预处理器 | `#800080` | `#c586c0` |
| 类型/内建 | `#2b91af` | `#4ec9b0` |

## 当前限制

- Lexer 暗色适配覆盖了 style 1-16 的常见分类，但个别语言的特殊 style（如 HTML 标签、Markdown 扩展语法等）可能仍有浅色残留
- Markdown 预览的主题跟随在预览已打开时不自动刷新，需要关闭再重新预览
- 查找/替换高亮指示器颜色在主题切换后需要下次搜索时才更新
