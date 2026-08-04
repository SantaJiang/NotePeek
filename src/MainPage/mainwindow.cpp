#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "apppreferences.h"
#include "preferencesdialog.h"
#include "themecolors.h"
#include <QFileDialog>
#include <QScreen>
#include <QFileInfo>
#include <QDir>
#include <QScrollBar>
#include <QMessageBox>
#include <QIcon>
#include <QCloseEvent>
#include <QEvent>
#include <QCoreApplication>
#include <QStyleHints>
#include <QApplication>
#include <QProcess>
#include <QScopedValueRollback>
#include <QTimer>
#include <QDockWidget>
#include <QMenu>
#include <QSplitter>
#include "editor/documentformat.h"
#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/documentpath.h"
#include "editor/editormanager.h"
#include "editor/documentsavecoordinator.h"
#include "editor/editorpane.h"
#include "editor/externalfilewatcher.h"
#include "editor/spliteditorcoordinator.h"
#include "tool/format/codeformatter.h"
#include "widgets/fileexplorerdock.h"
#include "widgets/searchresultsdock.h"

namespace {

QString eolText(QsciScintilla::EolMode mode)
{
    switch (mode)
    {
    case QsciScintilla::EolWindows:
        return QStringLiteral("\r\n");
    case QsciScintilla::EolMac:
        return QStringLiteral("\r");
    case QsciScintilla::EolUnix:
        break;
    }
    return QStringLiteral("\n");
}

QString normalizedAbsolutePath(const QString &path)
{
    return DocumentPath::normalized(path);
}

bool isPathInsideDirectory(const QString &path, const QString &directory)
{
    return DocumentPath::isInsideDirectory(path, directory);
}

void applyEditorSplitterStyle(QSplitter *splitter)
{
    if (!splitter)
    {
        return;
    }

    splitter->setHandleWidth(1);
    const QString styleSheet = QStringLiteral(
                                "QSplitter::handle:horizontal {"
                                "  width: 1px;"
                                "  background-color: %1;"
                                "  border: 0;"
                                "  margin: 0;"
                                "  padding: 0;"
                                "}").arg(ThemeColors::EditorTopBar::border().name());
    if (splitter->styleSheet() != styleSheet)
    {
        splitter->setStyleSheet(styleSheet);
    }
}

void applyMainWindowSeparatorStyle(QMainWindow *window)
{
    if (!window)
    {
        return;
    }

    const QString separatorColor = ThemeColors::EditorTopBar::border().name();
    const QString styleSheet = QStringLiteral(
                              "QMainWindow::separator {"
                              "  background-color: %1;"
                              "  width: 1px;"
                              "  height: 1px;"
                              "  border: 0;"
                              "  margin: 0;"
                              "  padding: 0;"
                              "}"
                              "QMainWindow::separator:hover {"
                              "  background-color: %1;"
                              "}")
                              .arg(separatorColor);
    if (window->styleSheet() != styleSheet)
    {
        window->setStyleSheet(styleSheet);
    }
}

} // namespace

MainWindow::MainWindow(QWidget *parent,
                       const QStringList &initialFiles,
                       bool singleFileWindow)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_fileExplorerDock(nullptr)
    , m_saveCoordinator(new DocumentSaveCoordinator(this))
    , m_externalFileWatcher(new ExternalFileWatcher(this))
    , m_editorCoordinator(nullptr)
    , m_singleFileWindow(false)
    , m_fileExplorerRootExplicit(false)
    , m_findReplaceManager(new FindReplaceManager(this))
    , m_headerSourceManager(new HeaderSourceManager(this))
    , m_markdownPreviewer(new MarkdownPreviewer(this))
{
    ui->setupUi(this);
    setAcceptDrops(false); // Drag and drop is handled by the editors.

    const bool hasFileArg = !initialFiles.isEmpty();
    m_singleFileWindow = singleFileWindow;

    const QList<EditorPane *> panes = { ui->editorPane, ui->editorPaneRight };
    for (int paneIndex = 0; paneIndex < panes.size(); ++paneIndex)
    {
        // A default blank document only makes sense in the first pane and
        // when no file was requested.
        auto *controller = new DocumentController(panes.at(paneIndex),
                                                  paneIndex == 0 && !hasFileArg,
                                                  m_saveCoordinator,
                                                  m_externalFileWatcher,
                                                  this);
        // Every pane needs the previewer so the "open Markdown in preview"
        // preference can render on open.
        controller->setMarkdownPreviewer(m_markdownPreviewer);
        // Each pane's side-preview toolbar button toggles the preview for its
        // own pane, mirroring the Tools ▸ Markdown ▸ Side Preview action.
        connect(controller->pane(), &EditorPane::sidePreviewToggleRequested,
                this, [this, controller]() { toggleMarkdownSidePreviewFor(controller); });
        m_documentControllers.append(controller);
    }

    ui->widget_findreplace->setFindReplaceManager(m_findReplaceManager);

    // Must exist before the first updateToolActions(), which enables/disables it.
    setupEncodingMenu();
    // Lets the Format Code entry explain which formatter it would run, and why
    // it is disabled when it is.
    ui->menuTool->setToolTipsVisible(true);

    m_fileExplorerDock = new FileExplorerDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileExplorerDock->dockWidget());
    connect(m_fileExplorerDock, &FileExplorerDock::fileActivated,
            this, [this](const QString &filePath) {
                activeController()->openFile(filePath);
            });
    connect(m_fileExplorerDock, &FileExplorerDock::openInNewWindowRequested,
            this, &MainWindow::openFileInNewWindow);

    m_searchResultsDock = new SearchResultsDock(this);
    addDockWidget(Qt::BottomDockWidgetArea, m_searchResultsDock->dockWidget());
    m_searchResultsDock->setVisible(false);
    connect(m_searchResultsDock, &SearchResultsDock::matchActivated,
            this, &MainWindow::openFileAtSearchMatch);
    // Search-result jumps into files that are still loading complete once the
    // controller reports the file as opened.
    for (DocumentController *controller : m_documentControllers)
    {
        // Editors are read-only while their file loads, which gates the tool
        // actions; they have to be re-evaluated once the load completes.
        connect(controller, &DocumentController::fileOpened,
                this, [this](CustomEditor *, const QString &) { updateToolActions(); });
        connect(controller, &DocumentController::fileOpened,
                this, [this](CustomEditor *editor, const QString &filePath) {
                    if (!m_pendingSearchJump.active
                        || !DocumentPath::equivalent(filePath, m_pendingSearchJump.filePath))
                    {
                        return;
                    }
                    m_pendingSearchJump.active = false;
                    jumpToEditorLocation(editor,
                                         m_pendingSearchJump.line,
                                         m_pendingSearchJump.column,
                                         m_pendingSearchJump.length);
                });
        connect(controller, &DocumentController::fileOpenFailed,
                this, [this](const QString &filePath) {
                    if (m_pendingSearchJump.active
                        && DocumentPath::equivalent(filePath, m_pendingSearchJump.filePath))
                    {
                        m_pendingSearchJump.active = false;
                    }
                });
    }

    // Seed the file-tree visibility from the preference.  This only sets the
    // startup state; the user's manual toggles during a session are never
    // overwritten by applying preferences later.
    const bool showFileTree = AppPreferences::instance().showFileTreeOnStartup();
    m_fileExplorerDock->setVisible(showFileTree);
    ui->action_showSlideBar->setChecked(showFileTree);
    ui->widget_findreplace->setVisible(false);

    ui->editorSplitter->setStretchFactor(0, 1);
    ui->editorSplitter->setStretchFactor(1, 1);
    applyEditorSplitterStyle(ui->editorSplitter);
    applyMainWindowSeparatorStyle(this);
    m_editorCoordinator = new SplitEditorCoordinator(m_documentControllers,
                                                     ui->editorSplitter,
                                                     m_singleFileWindow,
                                                     m_externalFileWatcher,
                                                     this);
    connect(m_editorCoordinator, &SplitEditorCoordinator::activeControllerChanged,
            this, &MainWindow::applyActiveController);
    connect(m_editorCoordinator, &SplitEditorCoordinator::paneModificationChanged,
            this, [this](DocumentController *controller, bool modified) {
                if (controller == activeController())
                {
                    updateWindowTitle(modified);
                }
            });
    connect(m_editorCoordinator, &SplitEditorCoordinator::paneCopyAvailableChanged,
            this, [this](DocumentController *controller, bool available) {
                if (controller == activeController())
                {
                    updateEditActions(available);
                }
            });
    connect(m_editorCoordinator, &SplitEditorCoordinator::filesDropped,
            this, [this](DocumentController *, const QList<QUrl> &urls) {
                onFileDraged(urls);
            });
    connect(m_editorCoordinator, &SplitEditorCoordinator::externalFileConflict,
            this, [this](const QString &filePath, bool deleted) {
                auto *message = new QMessageBox(QMessageBox::Warning,
                                                tr("External File Conflict"),
                                                deleted
                                                    ? tr("%1 was deleted outside NotePeek. The in-memory copy was kept and marked modified.")
                                                          .arg(filePath)
                                                    : tr("%1 changed outside NotePeek and could not be reloaded safely. Review the in-memory copy before overwriting it.")
                                                          .arg(filePath),
                                                QMessageBox::Ok,
                                                this);
                message->setAttribute(Qt::WA_DeleteOnClose);
                message->setModal(false);
                message->show();
            });
    applyActiveController(m_documentControllers.value(0));
    if (m_singleFileWindow)
    {
        ui->editorPane->setTopbarVisible(false);
        ui->editorPaneRight->setTopbarVisible(false);
    }

    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (!now)
            return;

        m_editorCoordinator->updateActivePaneForFocus(now);
    });

    connect(ui->widget_findreplace, &FindReplaceWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible)
        {
            m_findReplaceManager->clearHighlights();
        }
    });

    connect(ui->action_cut, &QAction::triggered, this, &MainWindow::on_action_cut_triggered);
    connect(ui->action_copy, &QAction::triggered, this, &MainWindow::on_action_copy_triggered);
    connect(ui->action_paste, &QAction::triggered, this, &MainWindow::on_action_paste_triggered);

    updateEditActions(false);
    updateToolActions();

    for (const QString &filePath : initialFiles)
    {
        if (!filePath.isEmpty())
            openFileByArg(filePath);
    }

    // Listen for system theme changes (light ↔ dark) at runtime.
    // QEvent::PaletteChange alone is unreliable on macOS with the Fusion style,
    // so we also connect to the dedicated colorSchemeChanged signal (Qt 6.5+).
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this]() {
                scheduleChromeThemeRefresh();
            });
#endif

    connect(m_findReplaceManager, &FindReplaceManager::searchResultsChanged, this, &MainWindow::onSearchResultsChanged);
}

MainWindow::~MainWindow()
{
    // Controllers may synchronously drain background saves in their
    // destructors. Destroy them while their panes and the shared save
    // coordinator are still alive, independent of QObject child-list order.
    delete m_editorCoordinator;
    m_editorCoordinator = nullptr;
    qDeleteAll(m_documentControllers);
    m_documentControllers.clear();
    delete ui;
}

void MainWindow::openFileByArg(const QString &fileName)
{
    openFileAndSetRootDirectory(fileName);
}

void MainWindow::setupEncodingMenu()
{
    // Tools ▸ Encoding.  Reopen reinterprets the on-disk bytes; convert
    // changes the target format and saves the document in it.
    m_encodingMenu = new QMenu(tr("Encoding"), this);
    m_reopenEncodingMenu = m_encodingMenu->addMenu(tr("Reopen with Encoding"));
    m_convertEncodingMenu = m_encodingMenu->addMenu(tr("Convert to Encoding and Save"));

    // Unicode first, then the legacy code pages (only present when this build
    // has codec support for them), separated so the common choices stay on top.
    bool separatorPending = false;
    for (DocumentEncoding encoding : DocumentFormatCodec::supportedEncodings())
    {
        if (DocumentFormatCodec::isLegacyEncoding(encoding) && !separatorPending)
        {
            separatorPending = true;
            m_reopenEncodingMenu->addSeparator();
            m_convertEncodingMenu->addSeparator();
        }

        const QString name = encoding == DocumentEncoding::Utf8Bom
                                 ? QStringLiteral("UTF-8 with BOM")
                                 : DocumentFormatCodec::encodingName(encoding);
        // UTF-8 with BOM reads identically to UTF-8, so it is offered only as a
        // conversion target, not as a way to reopen.
        if (encoding != DocumentEncoding::Utf8Bom)
        {
            m_reopenEncodingMenu->addAction(name, this, [this, encoding]() {
                activeController()->reopenCurrentWithEncoding(encoding);
            });
        }

        QAction *convertAction = m_convertEncodingMenu->addAction(name, this, [this, encoding]() {
            activeController()->convertEncodingAndSave(encoding);
        });
        convertAction->setCheckable(true);
        convertAction->setData(int(encoding));
        m_convertEncodingActions.append(convertAction);
    }

    // The current encoding is only known when the menu opens.
    connect(m_encodingMenu, &QMenu::aboutToShow, this, [this]() {
        CustomEditor *editor = activeController()->currentEditor();
        const int currentEncoding = editor ? int(editor->documentFormat().encoding) : -1;
        for (QAction *action : m_convertEncodingActions)
        {
            action->setChecked(action->data().toInt() == currentEncoding);
        }
        m_reopenEncodingMenu->setEnabled(editor && !editor->filePath().isEmpty());
    });
    ui->menuTool->addMenu(m_encodingMenu);
}

DocumentController *MainWindow::activeController() const
{
    return m_editorCoordinator ? m_editorCoordinator->activeController()
                               : m_documentControllers.value(0);
}

void MainWindow::applyActiveController(DocumentController *controller)
{
    if (!controller)
        return;

    CustomEditor *editor = controller->currentEditor();
    ui->statusbar->connectToEditor(editor);
    m_findReplaceManager->setCurrentEditor(editor);
    controller->attachFindReplaceManager(m_findReplaceManager);

    updateToolActions();
    updateEditActions(editor && editor->hasSelectedText());
    if (editor)
    {
        updateWindowTitle(editor->isModified());

        const QString filePath = editor->filePath();
        if (!filePath.isEmpty())
        {
            syncFileExplorerToFile(filePath, false);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_editorCoordinator->closeAllEditors())
    {
        event->accept();
    }
    else
    {
        event->ignore();
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::PaletteChange)
    {
        scheduleChromeThemeRefresh();
    }
}

void MainWindow::scheduleChromeThemeRefresh()
{
    if (m_chromeThemeRefreshPending)
    {
        return;
    }

    m_chromeThemeRefreshPending = true;
    QTimer::singleShot(0, this, [this]() {
        reapplyChromeTheme();
        m_chromeThemeRefreshPending = false;
    });
}

void MainWindow::reapplyChromeTheme()
{
    if (m_reapplyingChromeTheme)
    {
        return;
    }

    const QScopedValueRollback<bool> guard(m_reapplyingChromeTheme, true);
    applyEditorSplitterStyle(ui->editorSplitter);
    applyMainWindowSeparatorStyle(this);
    m_fileExplorerDock->reapplyTheme();
    m_editorCoordinator->reapplyThemeToEditorPanes();
    m_editorCoordinator->refreshVisibleMarkdownPreviews();
}

void MainWindow::refreshHighlights()
{
    if (ui->widget_findreplace->isVisible())
    {
        QTimer::singleShot(0, this, [=]
        {
            ui->widget_findreplace->refreshHighlights();
        });
    }
}

void MainWindow::setRootDirectory(const QString &dirPath, bool explicitDirectoryRoot)
{
    m_fileExplorerRootDirectory = normalizedAbsolutePath(dirPath);
    m_fileExplorerRootExplicit = explicitDirectoryRoot && !m_fileExplorerRootDirectory.isEmpty();
    m_fileExplorerDock->setRootDirectory(dirPath);
    m_searchResultsDock->setSearchRoot(m_fileExplorerRootDirectory);
}

void MainWindow::syncFileExplorerToFile(const QString &filePath, bool allowImplicitRootChange)
{
    if (filePath.isEmpty())
    {
        return;
    }

    if (m_fileExplorerRootDirectory.isEmpty())
    {
        if (allowImplicitRootChange)
        {
            const QFileInfo fileInfo(filePath);
            setRootDirectory(fileInfo.absolutePath(), false);
            m_fileExplorerDock->selectFile(filePath);
        }
        return;
    }

    if (isPathInExplorerRoot(filePath))
    {
        m_fileExplorerDock->selectFile(filePath);
        return;
    }

    if (!m_fileExplorerRootExplicit && allowImplicitRootChange)
    {
        const QFileInfo fileInfo(filePath);
        setRootDirectory(fileInfo.absolutePath(), false);
        m_fileExplorerDock->selectFile(filePath);
        return;
    }

    m_fileExplorerDock->clearSelection();
}

bool MainWindow::isPathInExplorerRoot(const QString &filePath) const
{
    return isPathInsideDirectory(filePath, m_fileExplorerRootDirectory);
}

void MainWindow::openFileAndSetRootDirectory(const QString &fileName)
{
    activeController()->openFile(fileName);
    syncFileExplorerToFile(fileName, true);
}

void MainWindow::on_action_new_triggered()
{
    activeController()->newDocument();
}

void MainWindow::on_action_openFile_triggered()
{
    const QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"));
    if (!fileName.isEmpty())
    {
        openFileAndSetRootDirectory(fileName);
    }
}

void MainWindow::on_action_openDirectory_triggered()
{
    DocumentController *controller = activeController();
    if (controller->maybeSave(controller->currentEditor()))
    {
        const QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Directory..."), ".");
        if (!dirPath.isEmpty())
        {
            setRootDirectory(dirPath, true);
            m_fileExplorerDock->setVisible(true);
            ui->action_showSlideBar->setChecked(true);
            m_fileExplorerDock->expandDirectory(dirPath);
        }
    }
}

bool MainWindow::on_action_save_triggered()
{
    return activeController()->save();
}

bool MainWindow::on_action_saveAs_triggered()
{
    return activeController()->saveAs();
}

void MainWindow::on_action_exit_triggered()
{
    this->close();
}

void MainWindow::on_action_about_triggered()
{
    QMessageBox aboutBox(QMessageBox::NoIcon,
                         QString(),
                         QStringLiteral("<h3>%1 v%2</h3>"
                                        "<p>%3</p>"
                                        R"(<p>%4</p>)")
                             .arg(QApplication::applicationDisplayName(),
                                  QApplication::applicationVersion(),
                                  "Copyright © 2026 Santa.",
                                  tr("For Yueyue")),
                         QMessageBox::Ok,
                         this);
    const QIcon appIcon(QStringLiteral(":/resource/notepeek.svg"));
    if (!appIcon.isNull())
    {
        aboutBox.setIconPixmap(appIcon.pixmap(QSize(96, 96)));
    }
    aboutBox.setWindowModality(Qt::WindowModal);
    aboutBox.exec();
}

void MainWindow::on_action_showSlideBar_triggered(bool checked)
{
    m_fileExplorerDock->setVisible(checked);
}

void MainWindow::on_action_preferences_triggered()
{
    PreferencesDialog dialog(this);
    connect(&dialog, &PreferencesDialog::applied, this, &MainWindow::applyPreferences);
    dialog.exec();
}

void MainWindow::applyPreferences()
{
    // Fonts and code-editing features apply live to every open editor in both
    // panes, then visible Markdown previews re-render with the new fonts.  The
    // file-tree setting applies at next launch and the Markdown default applies
    // to files opened afterwards, as before.
    m_editorCoordinator->applyFontPreferencesToEditors();
    m_editorCoordinator->applyCodeFeaturePreferencesToEditors();
    m_editorCoordinator->reapplyThemeToEditorPanes();
    m_editorCoordinator->refreshVisibleMarkdownPreviews();
    if (CustomEditor *editor = activeController()->currentEditor())
    {
        updateEditActions(editor->hasSelectedText());
    }
}

void MainWindow::on_action_undo_triggered()
{
    DocumentController *controller = activeController();
    if (CustomEditor *editor = controller->currentEditor())
    {
        editor->undo();
    }
}

void MainWindow::on_action_redo_triggered()
{
    DocumentController *controller = activeController();
    if (CustomEditor *editor = controller->currentEditor())
    {
        editor->redo();
    }
}

void MainWindow::on_action_cut_triggered()
{
    DocumentController *controller = activeController();
    if (CustomEditor *editor = controller->currentEditor())
    {
        editor->cut();
    }
}

void MainWindow::on_action_copy_triggered()
{
    activeController()->pane()->copyCurrentSelection();
}

void MainWindow::on_action_paste_triggered()
{
    DocumentController *controller = activeController();
    if (CustomEditor *editor = controller->currentEditor())
    {
        editor->paste();
    }
}

void MainWindow::on_action_find_triggered()
{
    DocumentController *controller = activeController();
    CustomEditor *editor = controller->currentEditor();
    if (!editor)
        return;

    const QString selectedText = editor->selectedText();
    if (ui->widget_findreplace->isVisible())
    {
        ui->widget_findreplace->setFocus();
    }
    else
    {
        ui->widget_findreplace->showWithInitialText(selectedText);
    }
}

void MainWindow::on_action_findInFiles_triggered()
{
    // Without an explicit root, search where the current file lives.
    if (m_fileExplorerRootDirectory.isEmpty())
    {
        if (CustomEditor *editor = activeController()->currentEditor())
        {
            const QString filePath = editor->filePath();
            if (!filePath.isEmpty())
            {
                m_searchResultsDock->setSearchRoot(QFileInfo(filePath).absolutePath());
            }
        }
    }

    m_searchResultsDock->setVisible(true);
    CustomEditor *editor = activeController()->currentEditor();
    m_searchResultsDock->focusSearchInput(editor ? editor->selectedText() : QString());
}

void MainWindow::openFileAtSearchMatch(const QString &filePath, int line, int column, int length)
{
    DocumentController *controller = activeController();
    CustomEditor *existingEditor = controller->editorManager()->getEditorByFilePath(filePath);
    if (existingEditor && !controller->isEditorLoading(existingEditor))
    {
        controller->openFile(filePath);
        jumpToEditorLocation(existingEditor, line, column, length);
    }
    else
    {
        m_pendingSearchJump = { filePath, line, column, length, true };
        controller->openFile(filePath);
    }
    syncFileExplorerToFile(filePath, false);
}

void MainWindow::jumpToEditorLocation(CustomEditor *editor, int line, int column, int length)
{
    if (!editor)
        return;

    const int lastLine = qMax(0, editor->lines() - 1);
    const int targetLine = qBound(0, line, lastLine);
    const int targetColumn = qBound(0, column, editor->lineLength(targetLine));
    editor->ensureLineVisible(targetLine);
    if (length > 0)
    {
        const int targetEnd = qBound(targetColumn,
                                     targetColumn + length,
                                     editor->lineLength(targetLine));
        editor->setSelection(targetLine, targetColumn, targetLine, targetEnd);
    }
    else
    {
        editor->setCursorPosition(targetLine, targetColumn);
    }
    editor->setFocus();
}

void MainWindow::on_action_formatCode_triggered()
{
    DocumentController *controller = activeController();
    CustomEditor *editor = controller->currentEditor();
    if (!editor || controller->pane()->isCurrentEditorHidden() || editor->isReadOnly())
        return;

    const QString filePath = editor->filePath();
    if (!CodeFormatter::canFormat(filePath))
        return;

    CodeFormatter::Options options;
    // indentationWidth() is 0 when the editor follows the tab width.
    options.indentWidth = editor->indentationWidth() > 0 ? editor->indentationWidth()
                                                         : editor->tabWidth();
    options.useTabs = editor->indentationsUseTabs();
    options.eol = eolText(editor->eolMode());

    const QString original = editor->text();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const CodeFormatter::Result result = CodeFormatter::format(original, filePath, options);
    QApplication::restoreOverrideCursor();

    if (!result.ok)
    {
        QMessageBox::warning(this,
                             tr("Format Code"),
                             tr("Could not format %1.\n\n%2")
                                 .arg(QFileInfo(filePath).fileName(), result.error));
        return;
    }

    // Replacing identical text would still push an undo step and mark the
    // document modified, so an already-formatted file is left alone.
    if (result.text == original)
        return;

    int line = 0;
    int index = 0;
    editor->getCursorPosition(&line, &index);
    const int firstVisibleLine =
        int(editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE));

    // selectAll()/replaceSelectedText() rather than setText(): the latter
    // empties the undo buffer, and formatting has to stay undoable in one step.
    editor->beginUndoAction();
    editor->selectAll();
    editor->replaceSelectedText(result.text);
    editor->endUndoAction();

    const int lastLine = qMax(0, editor->lines() - 1);
    const int targetLine = qBound(0, line, lastLine);
    editor->setCursorPosition(targetLine, qBound(0, index, editor->lineLength(targetLine)));
    editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
    editor->setFocus();
}

void MainWindow::on_action_switchHeaderSource_triggered()
{
    DocumentController *controller = activeController();
    CustomEditor *editor = controller->currentEditor();
    if (!editor)
        return;

    const QString currentFilePath = editor->filePath();
    if (currentFilePath.isEmpty())
        return;

    const QString correspondingFile = m_headerSourceManager->getCorrespondingFile(currentFilePath);
    if (!correspondingFile.isEmpty())
    {
        controller->openFile(correspondingFile);
        syncFileExplorerToFile(correspondingFile, false);
    }
}

void MainWindow::on_action_markdownPreview_triggered()
{
    toggleMarkdownSidePreviewFor(activeController());
}

void MainWindow::toggleMarkdownSidePreviewFor(DocumentController *controller)
{
    if (!controller)
        return;

    CustomEditor *editor = controller->currentEditor();
    if (!editor)
        return;

    if (!m_markdownPreviewer->isMarkdownFile(editor->filePath()))
        return;

    const bool previewVisible = controller->pane()->toggleMarkdownPreview(m_markdownPreviewer);
    if (previewVisible)
    {
        ui->widget_findreplace->setVisible(false);
    }

    controller->pane()->updateSidePreviewButton();
    updateEditActions(editor->hasSelectedText());
    updateToolActions();
}

void MainWindow::on_action_markdownExportPdf_triggered()
{
    CustomEditor *editor = activeController()->currentEditor();
    if (!editor)
        return;

    const QString currentFilePath = editor->filePath();
    if (!m_markdownPreviewer->isMarkdownFile(currentFilePath))
        return;

    const QFileInfo fileInfo(currentFilePath);
    const QString defaultPath = fileInfo.absolutePath()
                                + QDir::separator()
                                + fileInfo.completeBaseName()
                                + QStringLiteral(".pdf");
    QString outputPath = QFileDialog::getSaveFileName(this,
                                                      tr("Export Markdown to PDF"),
                                                      defaultPath,
                                                      tr("PDF Files (*.pdf)"));
    if (outputPath.isEmpty())
    {
        return;
    }

    if (QFileInfo(outputPath).suffix().isEmpty())
    {
        outputPath += QStringLiteral(".pdf");
    }

    if (!m_markdownPreviewer->exportToPdf(editor->text(), currentFilePath, outputPath))
    {
        QMessageBox::warning(this,
                             tr("Export Failed"),
                             tr("Could not export Markdown to PDF."));
    }
}

void MainWindow::onFileDraged(const QList<QUrl> &urls)
{
    for (const QUrl &url : urls)
    {
        if (url.isLocalFile())
        {
            openFileByArg(url.toLocalFile());
        }
    }
}

void MainWindow::openFileInNewWindow(const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    QProcess::startDetached(QCoreApplication::applicationFilePath(),
                            { QStringLiteral("--single-file-window"), filePath });
}

void MainWindow::updateEditActions(bool available)
{
    DocumentController *controller = activeController();
    CustomEditor *editor = controller->currentEditor();
    // In Markdown preview-only mode the source editor is hidden; actions that
    // would edit or search it are disabled so they cannot act on unseen text.
    // Copy stays governed by `available`, which already reflects a preview
    // selection, so text can still be copied out of the preview.
    const bool sourceEditable = editor != nullptr
                                && !controller->pane()->isCurrentEditorHidden();
    ui->action_cut->setEnabled(sourceEditable && available);
    ui->action_copy->setEnabled(available);
    ui->action_paste->setEnabled(sourceEditable);
    ui->action_undo->setEnabled(sourceEditable);
    ui->action_redo->setEnabled(sourceEditable);
    ui->action_find->setEnabled(sourceEditable);
}

void MainWindow::updateWindowTitle(bool modified)
{
    CustomEditor *editor = activeController()->currentEditor();
    if (!editor)
        return;

    const QString filePath = editor->filePath();
    const QString title = filePath.isEmpty() ? QStringLiteral("untitled") : QFileInfo(filePath).fileName();

    const QString appName = tr("NotePeek");
    if (modified)
    {
        setWindowTitle(tr("%1 - %2[*]").arg(appName, title));
        setWindowModified(true);
    }
    else
    {
        setWindowTitle(tr("%1 - %2").arg(appName, title));
        setWindowModified(false);
    }
}

void MainWindow::updateToolActions()
{
    DocumentController *controller = activeController();
    CustomEditor *editor = controller->currentEditor();
    const QString filePath = editor ? editor->filePath() : QString();
    const bool previewVisible = controller->pane()->isCurrentEditorPreviewVisible();

    // The find/replace bar operates on the source editor.  When the source is
    // hidden (Markdown preview-only) collapse it so it is not bound to an
    // editor the user cannot see.
    if (controller->pane()->isCurrentEditorHidden() && ui->widget_findreplace->isVisible())
    {
        ui->widget_findreplace->setVisible(false);
    }

    const bool cppSupported = !filePath.isEmpty() && m_headerSourceManager->isCppFile(filePath);
    const bool markdownSupported = !filePath.isEmpty() && m_markdownPreviewer->isMarkdownFile(filePath);

    // A formatter has to exist for the language *and* be usable: C-family files
    // report false while clang-format is not installed, so the entry is never
    // offered when clicking it could only produce an error.
    // A file that is still loading is held read-only, and formatting it would
    // silently do nothing, so availability follows the editor's writability.
    const bool sourceEditable = editor != nullptr
                                && !controller->pane()->isCurrentEditorHidden()
                                && !editor->isReadOnly();
    const bool formatSupported = sourceEditable && CodeFormatter::canFormat(filePath);
    ui->action_formatCode->setEnabled(formatSupported);
    ui->action_formatCode->setToolTip(
        formatSupported ? tr("Format this file with %1").arg(CodeFormatter::formatterName(filePath))
                        : tr("No code formatter is available for this file."));

    ui->action_switchHeaderSource->setEnabled(cppSupported && !previewVisible);
    ui->menuToolCpp->setEnabled(cppSupported && !previewVisible);
    ui->action_markdownPreview->setEnabled(markdownSupported);
    ui->action_markdownPreview->setChecked(markdownSupported && previewVisible);
    ui->action_markdownExportPdf->setEnabled(markdownSupported);
    ui->menuToolMarkdown->setEnabled(markdownSupported);
    if (m_encodingMenu)
    {
        m_encodingMenu->setEnabled(editor != nullptr
                                   && !controller->pane()->isCurrentEditorHidden());
    }
    ui->menuTool->setEnabled(true);

    controller->pane()->updateSidePreviewButton();
}

void MainWindow::onSearchResultsChanged()
{
    CustomEditor *editor = activeController()->currentEditor();
    if (editor)
    {
        editor->verticalScrollBar()->update();
    }
}
