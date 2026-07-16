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
#include <QCloseEvent>
#include <QEvent>
#include <QCoreApplication>
#include <QStyleHints>
#include <QApplication>
#include <QProcess>
#include <QDockWidget>
#include <QSplitter>
#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/documentpath.h"
#include "editor/documentsavecoordinator.h"
#include "editor/editorpane.h"
#include "editor/externalfilewatcher.h"
#include "editor/spliteditorcoordinator.h"
#include "widgets/fileexplorerdock.h"

namespace {

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
    splitter->setStyleSheet(QStringLiteral(
                                "QSplitter::handle:horizontal {"
                                "  width: 1px;"
                                "  background-color: %1;"
                                "  border: 0;"
                                "  margin: 0;"
                                "  padding: 0;"
                                "}").arg(ThemeColors::EditorTopBar::border().name()));
}

void applyMainWindowSeparatorStyle(QMainWindow *window)
{
    if (!window)
    {
        return;
    }

    const QString separatorColor = ThemeColors::EditorTopBar::border().name();
    window->setStyleSheet(QStringLiteral(
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
                              .arg(separatorColor));
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
    , m_leftController(nullptr)
    , m_rightController(nullptr)
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

    // A default blank document only makes sense when no file was requested.
    m_leftController = new DocumentController(ui->editorPane,
                                              !hasFileArg,
                                              m_saveCoordinator,
                                              m_externalFileWatcher,
                                              this);
    m_rightController = new DocumentController(ui->editorPaneRight,
                                               false,
                                               m_saveCoordinator,
                                               m_externalFileWatcher,
                                               this);

    // Both panes need the previewer so the "open Markdown in preview" preference
    // can render on open.
    m_leftController->setMarkdownPreviewer(m_markdownPreviewer);
    m_rightController->setMarkdownPreviewer(m_markdownPreviewer);

    // Each pane's side-preview toolbar button toggles the preview for its own
    // pane, mirroring the Tools ▸ Markdown ▸ Side Preview action.
    connect(m_leftController->pane(), &EditorPane::sidePreviewToggleRequested,
            this, [this]() { toggleMarkdownSidePreviewFor(m_leftController); });
    connect(m_rightController->pane(), &EditorPane::sidePreviewToggleRequested,
            this, [this]() { toggleMarkdownSidePreviewFor(m_rightController); });

    ui->widget_findreplace->setFindReplaceManager(m_findReplaceManager);

    m_fileExplorerDock = new FileExplorerDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileExplorerDock->dockWidget());
    connect(m_fileExplorerDock, &FileExplorerDock::fileActivated,
            this, [this](const QString &filePath) {
                activeController()->openFile(filePath);
            });
    connect(m_fileExplorerDock, &FileExplorerDock::openInNewWindowRequested,
            this, &MainWindow::openFileInNewWindow);

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
    m_editorCoordinator = new SplitEditorCoordinator(m_leftController,
                                                     m_rightController,
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
    applyActiveController(m_leftController);
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
                applyEditorSplitterStyle(ui->editorSplitter);
                applyMainWindowSeparatorStyle(this);
                m_editorCoordinator->reapplyThemeToEditorPanes();
                m_editorCoordinator->refreshVisibleMarkdownPreviews();
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
    delete m_leftController;
    m_leftController = nullptr;
    delete m_rightController;
    m_rightController = nullptr;
    delete ui;
}

void MainWindow::openFileByArg(const QString &fileName)
{
    openFileAndSetRootDirectory(fileName);
}

DocumentController *MainWindow::activeController() const
{
    return m_editorCoordinator ? m_editorCoordinator->activeController() : m_leftController;
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
        applyEditorSplitterStyle(ui->editorSplitter);
        applyMainWindowSeparatorStyle(this);
        m_fileExplorerDock->reapplyTheme();
        m_editorCoordinator->reapplyThemeToEditorPanes();
        m_editorCoordinator->refreshVisibleMarkdownPreviews();
    }
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
    QMessageBox::about(this, QString(),
                       QStringLiteral("<h3>%1 v%2</h3>"
                                      "<p>%3</p>"
                                      R"(<p>%4</p>)")
                       .arg(QApplication::applicationDisplayName(), QApplication::applicationVersion(), "Copyright © 2026 Santa.", tr("For Yueyue")));
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

void MainWindow::on_action_foldCurrentBlock_triggered()
{
    if (CustomEditor *editor = activeController()->currentEditor())
    {
        editor->toggleCurrentFold();
    }
}

void MainWindow::on_action_toggleAllFolds_triggered()
{
    if (CustomEditor *editor = activeController()->currentEditor())
    {
        editor->toggleAllFolds();
    }
}

void MainWindow::on_action_goToMatchingBrace_triggered()
{
    if (CustomEditor *editor = activeController()->currentEditor())
    {
        editor->goToMatchingBrace();
    }
}

void MainWindow::on_action_completeWord_triggered()
{
    if (CustomEditor *editor = activeController()->currentEditor())
    {
        editor->completeWordFromDocument();
    }
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
    const bool codeEditable = sourceEditable && editor->codeEditingFeaturesEnabled();
    const bool foldingAvailable = codeEditable
                                  && editor->folding() != CustomEditor::NoFoldStyle;
    ui->action_cut->setEnabled(sourceEditable && available);
    ui->action_copy->setEnabled(available);
    ui->action_paste->setEnabled(sourceEditable);
    ui->action_undo->setEnabled(sourceEditable);
    ui->action_redo->setEnabled(sourceEditable);
    ui->action_find->setEnabled(sourceEditable);
    ui->action_foldCurrentBlock->setEnabled(foldingAvailable);
    ui->action_toggleAllFolds->setEnabled(foldingAvailable);
    ui->menuCodeFolding->setEnabled(foldingAvailable);
    ui->action_goToMatchingBrace->setEnabled(codeEditable && editor->bracketAssistanceEnabled());
    ui->action_completeWord->setEnabled(codeEditable && editor->documentCompletionEnabled());
}

void MainWindow::updateWindowTitle(bool modified)
{
    CustomEditor *editor = activeController()->currentEditor();
    if (!editor)
        return;

    const QString filePath = editor->filePath();
    const QString title = filePath.isEmpty() ? tr("Untitled") : QFileInfo(filePath).fileName();

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

    ui->action_switchHeaderSource->setEnabled(cppSupported && !previewVisible);
    ui->menuToolCpp->setEnabled(cppSupported && !previewVisible);
    ui->action_markdownPreview->setEnabled(markdownSupported);
    ui->action_markdownPreview->setChecked(markdownSupported && previewVisible);
    ui->action_markdownExportPdf->setEnabled(markdownSupported);
    ui->menuToolMarkdown->setEnabled(markdownSupported);
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
