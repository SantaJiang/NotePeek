# NotePeek 全局优化分析与实施计划

> 记录日期：2026-07-25
> 项目：`/Users/santa/Documents/GitHub/NotePeek`（Qt + QScintilla 跨平台代码/文本编辑器，v1.3.3）
> 本文档记录：本次会话的沟通内容、对项目的分析结论、决策过程、实施计划与最终落地情况。

---

## 一、会话背景与诉求

**你的原始诉求：** 从项目全局角度，从架构、性能、框架等方面分析当前项目，看还有哪些优化方案，以及下一步可以新增的功能。

**分析方法：** 并行派出两个分析代理，分别深入：
- 维度一：性能相关路径（文件加载/保存线程模型、Markdown 渲染策略、查找高亮成本、每按键开销、现有性能测试覆盖）
- 维度二：工程化与功能缺口（CI/CD、测试覆盖、偏好设置、i18n、主题、功能清单对标、技术欠账）

所有结论均带 `文件:行号` 出处，非泛泛而谈。

---

## 二、项目现状评估

### 总体判断

项目质量相当高：应用层约 1.2 万行，分层清晰（`MainWindow` → 协调器 → 编辑器服务 → 纯工具类），有架构文档与依赖策略文档，CI 覆盖三平台构建 + Qt 5.15/6.2 兼容性 + ASan/UBSan，文件 IO 已异步化且用 `QSaveFile` 保证原子写入，代码中**没有任何 TODO/FIXME 欠账**。

**主要短板不在架构，而在三处：** 残留的 UI 线程性能热点、工程化收尾环节、以及作为编辑器的功能广度。

### 2.1 性能维度发现

| # | 问题 | 出处 | 影响 |
|---|------|------|------|
| 1 | 加载完成后单次 `SCI_ADDTEXT` 同步灌入整个缓冲区 | `documentcontroller.cpp:342` | 100MB 级文件明显卡住界面 |
| 2 | 外部变更后的重新加载**完全同步**，未走异步路径 | `documentcontroller.cpp:842` | 外部改动大文件时阻塞 UI |
| 3 | 每次保存完成后重新做完整快照并逐字节比较 | `documentcontroller.cpp:470-475` | 每次保存 O(n) 拷贝+比较（UI 线程） |
| 4 | `text().isEmpty()` 为判空复制整个文档 | `documentcontroller.cpp:773`、`:1014` | 后者在每次打开文件时都执行 |
| 5 | UTF-16 编码保存是逐字符循环 | `documentformat.cpp:245-259` | 线性但常数大 |
| 6 | `replaceAll` 无撤销批处理、无重绘抑制 | `findreplacemanager.cpp:145-187` | 大文档全量替换慢，撤销需逐条回退 |
| 7 | 每次 `textChanged` 都转发 `modificationChanged` | `editormanager.cpp:409` | 下游页标题读写、外部变更检查每按键触发 |
| 8 | 滚动条装饰器在按键/光标/选区/滚动时全量重绘 | `highlightedscrollbardecorator.cpp` | O(matches) per paint |
| 9 | 查找高亮的大文件豁免用的是**磁盘**文件大小 | `findreplacemanager.cpp:364` | 未保存的超大缓冲区不受保护 |
| 10 | watcher 内 `files().contains()` 每轮重建列表 | `externalfilewatcher.cpp:124-138` | O(n²)，标签页多时放大 |
| 11 | Markdown 预览每次防抖后全文 `text()` 拷贝 | `documentpage.cpp:365` | 2MB 以上直接禁用预览 |

**已做对的部分（保持不动）：** 加载走 2 线程池、保存走独立线程池 + 按路径 FIFO 串行化 + 快照合并、UTF-8/LF 快速路径直接 memcpy 无 QString 往返、Markdown 渲染 180ms 防抖 + 单线程池 + 请求合并、大文件（>10MB）已有降级策略（禁 lexer/禁换行/预分配）。

### 2.2 架构维度发现

- **双 pane 硬编码是最大结构性限制**：`MainWindow` 直接持有 `m_leftController`/`m_rightController`，`SplitEditorCoordinator` 假定恰好两个。未来多分屏/拖拽分屏需先解开。
- **同步等待用嵌套 `QEventLoop`**（`documentcontroller.cpp:601-638`）：能工作，但嵌套事件循环是重入 bug 的温床（关闭过程中再次触发文件监视回调等）。
- **主题硬编码在 `themecolors.h`**：每个颜色两个值（明/暗）、跟随系统，无用户可选主题。
- **Qt 5.15 兼容成本**：Qt5 已 EOL，CI 维持兼容矩阵有持续成本（如 `colorSchemeChanged` 仅 6.5+ 导致双路径）。

### 2.3 工程化维度发现

- **缺发布流水线**：CI 只有构建+测试，没有 tag 触发的打包发布（DMG/NSIS/DEB/RPM 全靠手动 cpack），而 CPack 配置是现成的。
- **测试盲区**：`mainwindow.cpp`、`preferencesdialog.cpp`、`widgets/` 全部、7 个自定义 lexer、`lexerfactory`、`headersourcemanager`、查找替换 UI 均零测试。性能测试未覆盖 Markdown 渲染延迟、查找高亮、50MB+ 文件。
- **i18n 只有 zh_CN**（157 条全译完），且无应用内语言切换，仅跟随系统 locale。
- **仓库卫生**：`.DS_Store`、`CMakeLists.txt.user` 模式已在 .gitignore 中，但 CTest 的 `Testing/` 目录未被忽略。

### 2.4 功能缺口（逐项 grep 验证均不存在）

多光标、列编辑、自动保存、会话恢复、最近文件、跳转到行、行操作（复制/移动/删除行）、行尾空白处理、编码手动选择、字体缩放快捷键、自动换行手动开关、打印、**项目内搜索**、书签（滚动条已能画标记但无命令接线——功能已完成一半）、minimap。

---

## 三、决策过程

我给出的建议分三档（近期/中期/远期），**你的决策是：**

| 范围 | 决策 |
|------|------|
| 一（性能优化） | ✅ 全部执行 |
| 二（架构与框架） | ✅ 执行，**但 Qt 5.15 必须保留**（你的 Linux 平台就是 Qt 5.15） |
| 三（工程化补齐） | ✅ 全部执行 |
| 四（新功能）近期项 | ❌ 全部不做（最近文件、会话恢复、跳转到行、字体缩放、换行开关、行操作、书签） |
| 四（新功能）中期项 | ✅ 仅做 **7 项目内搜索** 和 **9 编码手动选择** |
| 四（新功能）远期项 | ❌ 全部不做（LSP、Git 集成、插件机制） |

**关键约束（贯穿全部实施）：** 所有改动必须保持 Qt 5.15 兼容，不得引入 Qt6-only API。

---

## 四、实施计划

### 执行策略

按风险从低到高分四阶段推进，**每阶段都构建 + 跑测试验证**，避免把高风险重构和新功能混在一起排查：

```
阶段一：性能快赢（7 项，改动局部、可独立验证）
   ↓
阶段二：两个新功能（新增代码为主，不动既有逻辑）
   ↓
阶段三：工程化（CI/测试/i18n，不影响运行时行为）
   ↓
阶段四：架构重构（风险最高，放最后，行为保持不变）
```

### 阶段一：性能（7 项）

| # | 目标 | 实现方案 |
|---|------|----------|
| 1 | 大文件不冻结 UI | ①外部重载改由 `SplitEditorCoordinator` 后台读盘（`QtConcurrent`），控制器只负责应用已加载内容；同路径只允许一个读在飞，后续变更合并成一次重跑。②新增 `insertContentInChunks()`，超过每 tick 预算（8MB）的内容分块插入，块间让出事件循环 |
| 2 | 去掉保存后的 O(n) 校验 | 改用 `editor->isModified()` 判断——`saveFile()` 取快照时已清 modified 标志，写盘期间若用户编辑则标志为真，语义等价且 O(1) |
| 3 | 消除全文拷贝 | 判空改 `length()==0`；UTF-16 编码改一次 `resize` + 批量 `qToLittleEndian/qToBigEndian`；Markdown 预览与非快速路径保存改为直接读 `SCI_GETCHARACTERPOINTER` 转一次 |
| 4 | replaceAll 提速 | 包 `beginUndoAction/endUndoAction`（一次撤销）+ `setUpdatesEnabled(false)`（无逐匹配重绘） |
| 5 | 降低每按键开销 | `modificationChanged` 仅在状态真正翻转时转发（用 shared_ptr 记录上次状态）；滚动条重绘用 `m_updatePending` 标志合并到一次事件循环 |
| 6 | 补两个防护缺口 | 查找高亮改用内存缓冲区长度而非磁盘大小；watcher 一次性取出 watched 集合转 `QSet`，消除 O(n²) |
| 7 | 预览渲染优化 | 快照直接从 Scintilla 缓冲区转一次，省掉 `text()` 的双份拷贝 |

### 阶段二：新功能（2 项）

**A. 项目内搜索（Find in Files, `Ctrl+Shift+F`）**

设计要点：
- `find/FileSearcher`（core 层，无 UI 依赖）：`QDirIterator` 递归遍历 + 后台线程匹配，候选文件过 `SupportedFilePolicy` 过滤（跳过二进制/不支持类型），跳过 >10MB 文件，结果上限 2000 条（防内存与结果树爆炸），支持取消，仅最新一次搜索上报结果（generation 计数）
- `widgets/SearchResultsDock`（底部 dock）：查询行（关键词 + 区分大小写 + 全字匹配）、状态行、按文件分组的结果树，双击/回车发 `matchActivated`
- `MainWindow`：接线跳转——已打开的文件直接定位，未打开的先异步打开、待 `fileOpened` 到达再跳（`PendingLocationJump`），跳转时选中匹配文本
- 搜索根目录：优先用文件树的 root，未设置时退化为当前文件所在目录

**B. 编码手动选择（Tools ▸ Encoding）**

设计要点：
- `DocumentFormatCodec::decodeWithEncoding()`：绕过自动检测，按指定编码解码（匹配的 BOM 会被消费，UTF-8/UTF-8-BOM 解码等价、记录实际存在的 BOM）
- `DocumentIO::loadEditorFileWithEncoding()`：对应的加载入口
- `DocumentController::reopenCurrentWithEncoding()`：后台读盘 + 保留光标/滚动位置；有未保存改动时先确认
- `DocumentController::convertEncodingAndSave()`：改目标格式后走正常保存
- 菜单：「以指定编码重新打开」（UTF-8 / UTF-16 LE / UTF-16 BE）+「转换编码并保存」（含 UTF-8 with BOM），打开菜单时勾选当前编码

### 阶段三：工程化（3 项）

| # | 内容 |
|---|------|
| 工1 | 新增 tag(`v*`) 触发的 `release.yml`：三平台矩阵构建 → 跑测试 → cpack 出 DMG/NSIS/DEB+RPM → 附加到 GitHub Release；`.gitignore` 补 `/Testing/` |
| 工2 | 新增 `tests/newfeatures_test.cpp` 覆盖搜索、强制编码解码、UTF-16 往返、lexer 映射、头/源配对、查找高亮；性能测试补查找高亮与 Markdown 渲染两个基准 |
| 工3 | `AppPreferences` 加 `language` 键（空=跟随系统），偏好对话框加语言下拉（跟随系统/English/简体中文），`main.cpp` 调整加载顺序（先读偏好再装 translator），跑 lupdate 并补全新增字符串的中文翻译 |

### 阶段四：架构（3 项，行为保持不变）

| # | 内容 |
|---|------|
| 架1 | `SplitEditorCoordinator` 内部改为有序控制器列表 `QList<DocumentController*>`，双 pane 构造函数保留为便捷包装；`MainWindow` 改持 `m_documentControllers` 并用循环建控制器。为未来多分屏铺路 |
| 架2 | 保留嵌套 `QEventLoop`（改成状态机会牵动过多调用点），但加 `m_drainingSaves` 重入保护：排空期间拒绝重入的 open/close/reload/编码切换 |
| 架3 | `themecolors.h` 重构为单一数据表 `roleColors(ColorRole)`（每角色一组明/暗值），各 widget 命名空间退化为薄访问器。用户可选主题将来只需替换表查询，不动调用点。**明/暗取值与跟随系统行为完全不变** |

### 验证计划

1. 每阶段：`cmake --build` + `ctest` 全绿
2. 全部完成后：`-DNOTEPEEK_WARNINGS_AS_ERRORS=ON` 严格构建（对齐 CI）
3. ASan/UBSan Debug 构建 + 测试（对齐 CI sanitizer job，重点验证新增的异步/生命周期代码）
4. 真实应用启动烟测（打开文件，确认不崩）
5. Qt 5.15 兼容性：本地无法构建，逐项人工核对避开 Qt6-only API，最终由 CI 的 `qt-compatibility` job 确认

---

## 五、实施结果

### 落地清单（15 项全部完成，已逐项 grep 核实）

**性能** ①外部变更异步重载 + 大文件分块插入 ②保存后轻量校验 ③消除三处全文拷贝 + UTF-16 批量编码 ④replaceAll 撤销批处理 ⑤按键信号与滚动条重绘节流 ⑥查找高亮缓冲区防护 + watcher O(n²) 修复 ⑦预览快照少一次拷贝

**功能** ⑧项目内搜索（`FileSearcher` + `SearchResultsDock` + 跳转接线）⑨编码手动选择（重新打开/转换保存）

**工程化** ⑩release 流水线 + .gitignore ⑪新增 8 个测试用例 + 2 个性能基准 ⑫应用内语言选择 + 22 条新中文翻译

**架构** ⑬双 pane 抽象为控制器列表 ⑭保存排空重入保护 ⑮主题层数据驱动重构

**文档** `docs/architecture.md` 与 `README.md` 同步更新

代码规模：32 个文件修改，+1612/-456 行，新增 6 个文件。

### 验证结果

| 项目 | 结果 |
|------|------|
| 严格构建（warnings-as-errors） | ✅ 应用代码零警告 |
| 全部测试（4 个套件） | ✅ 100% 通过 |
| ASan/UBSan 构建 + 测试 | ✅ 100% 通过（新异步代码无内存错误） |
| 真实应用启动烟测 | ✅ 正常运行 |

### 过程中发现并修复的一个真实崩溃 ⚠️

烟测时应用**启动即段错误**（exit 139）。lldb 定位：
```
MainWindow::updateToolActions()   ← m_encodingMenu 为 nullptr
MainWindow::applyActiveController()
MainWindow::MainWindow()
```
原因：我把编码菜单建在了构造函数中 `applyActiveController()` **之后**，而后者会调用 `updateToolActions()` 去访问尚未创建的菜单指针。

修复：提取 `setupEncodingMenu()` 并前移到首次 `updateToolActions()` 之前，同时加空指针保护；架构文档中记录了这条构造顺序约束。

**这个崩溃没有被任何测试拦住**——因为 `MainWindow` 此前零测试覆盖，正是分析中指出的盲区。已补 `constructsMainWindowShell` 烟测（空窗口 + 带文件打开两种路径），今后这类"成员使用早于初始化"的问题会在 CI 暴露，而不是在用户面前。

---

## 六、遗留事项与后续建议

### 需要 CI 确认

- **Qt 5.15 兼容性**：本地仅有 Qt 6.11，无法实测。已逐项核对避开 Qt6-only API（`QList` 填充构造、`QString::utf16()` 返回类型、`QMenu::addAction(text, ctx, functor)` 可用性等），并为自定义类型补了 `qRegisterMetaType`（Qt5 的 `QSignalSpy` 需要）。最终由 CI 的 `qt-compatibility` job 判定。

### 未纳入本次范围

- 一条既有的无关警告：`Failed to load Noto Color Emoji from resource`（彩色位图字体被 macOS `addApplicationFont` 拒绝，资源本身存在且已注册），非本次改动引入。
- 你明确排除的功能项（近期 7 项 + 远期 3 项）。
- Markdown 预览的增量渲染（当前仍是全文档重新 parse + `setHtml` 全量重排，2MB 上限）——需要按块 diff，工作量较大，属独立课题。
- 剩余测试盲区：`preferencesdialog`、`widgets/` 多数类、7 个自定义 lexer。

### 若继续推进，建议优先级

1. 观察 CI 三平台（尤其 Qt 5.15）结果，修正兼容性问题
2. 打一个 tag 验证新 release 流水线端到端可用
3. Markdown 预览增量渲染（把 2MB 上限提上去）
4. 用户可选主题（主题层已就绪，只需接数据源 + 偏好项）
