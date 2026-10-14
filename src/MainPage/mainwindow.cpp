#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "apppreferences.h"
#include "preferencesdialog.h"
#include "platform/platformstyle.h"
#include <QFileDialog>
#include <QScreen>
#include <QFileInfo>
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
#include <QSet>
#include <QTimer>
#include <QDockWidget>
#include <QMenu>
#include <QSplitter>

#include <utility>
#include "editor/documentformat.h"
#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "foundation/documentpath.h"
#include "editor/editormanager.h"
#include "editor/documentsavecoordinator.h"
#include "editor/editorpane.h"
#include "editor/externalfilewatcher.h"
#include "editor/spliteditorcoordinator.h"
#include "find/searchnavigationcoordinator.h"
#include "tool/cpp/cppnavigationcoordinator.h"
#include "tool/format/codeformatter.h"
#include "tool/format/codeformattingcoordinator.h"
#include "tool/markdown/markdowntoolcoordinator.h"
#include "versioncontrol/gitpresentationcoordinator.h"
#include "widgets/fileexplorerdock.h"
#include "widgets/searchresultsdock.h"

namespace {

QString normalizedAbsolutePath(const QString &path)
{
    return DocumentPath::normalized(path);
}

bool isPathInsideDirectory(const QString &path, const QString &directory)
{
    return DocumentPath::isInsideDirectory(path, directory);
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
    , m_gitCoordinator(nullptr)
    , m_editorCoordinator(nullptr)
    , m_singleFileWindow(false)
    , m_fileExplorerRootExplicit(false)
    , m_findReplaceManager(new FindReplaceManager(this))
    , m_cppNavigation(nullptr)
    , m_codeFormattingCoordinator(nullptr)
    , m_markdownTools(new MarkdownToolCoordinator(this, this))
{
    ui->setupUi(this);
    m_codeFormattingCoordinator = new CodeFormattingCoordinator(
        this,
        [this](CustomEditor *editor) {
            DocumentController *controller = activeController();
            return controller && controller->currentEditor() == editor
                   && !controller->pane()->isCurrentEditorHidden();
        },
        this);
    connect(m_codeFormattingCoordinator,
            &CodeFormattingCoordinator::formattingStateChanged,
            this,
            &MainWindow::updateToolActions);
    m_cppNavigation = std::make_unique<CppNavigationCoordinator>(
        [this]() { return activeController(); },
        [this](const QString &filePath) { syncFileExplorerToFile(filePath, false); });
    connect(m_markdownTools, &MarkdownToolCoordinator::previewStateChanged,
            this, [this](CustomEditor *editor, bool visible) {
                if (visible)
                    ui->widget_findreplace->setVisible(false);
                updateEditActions(editor && editor->hasSelectedText());
                updateToolActions();
            });
    setAcceptDrops(false); // Drag and drop is handled by the editors.
    ui->statusbar->setVisible(AppPreferences::instance().showStatusBar());

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
        controller->setMarkdownPreviewer(m_markdownTools->previewer());
        connect(controller, &DocumentController::previewLayoutChanged,
                this, [this, controller]() {
                    if (controller != activeController())
                        return;
                    CustomEditor *editor = controller->currentEditor();
                    updateEditActions(editor && editor->hasSelectedText());
                    updateToolActions();
                });
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
    m_gitCoordinator = new GitPresentationCoordinator(m_fileExplorerDock,
                                                       ui->statusbar,
                                                       m_externalFileWatcher,
                                                       this);
    m_gitCoordinator->setEnabled(AppPreferences::instance().gitIntegrationEnabled());

    m_searchResultsDock = new SearchResultsDock(this);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_searchResultsDock->dockWidget());
    m_searchResultsDock->setVisible(false);
    m_searchNavigation = new SearchNavigationCoordinator(
        [this]() { return activeController(); },
        [this](const QString &filePath) { syncFileExplorerToFile(filePath, false); },
        this);
    connect(m_searchResultsDock, &SearchResultsDock::matchActivated,
            m_searchNavigation, &SearchNavigationCoordinator::openMatch);
    connect(ui->widget_findreplace, &FindReplaceWidget::advancedSearchRequested,
            this, &MainWindow::on_action_findInFiles_triggered);
    // Search-result jumps into files that are still loading complete once the
    // controller reports the file as opened.
    for (DocumentController *controller : m_documentControllers)
    {
        m_searchNavigation->observeController(controller);
        // Editors are read-only while their file loads, which gates the tool
        // actions; they have to be re-evaluated once the load completes.
        connect(controller, &DocumentController::fileOpened,
                this, [this](CustomEditor *, const QString &) {
                    updateToolActions();
                    m_gitCoordinator->requestRefresh();
                });
        connect(controller, &DocumentController::editorModificationChanged,
                this, [this](CustomEditor *, bool modified) {
                    if (!modified)
                        m_gitCoordinator->requestRefresh();
                });
        connect(controller, &DocumentController::filePathChanged,
                this,
                [this](CustomEditor *, const QString &, const QString &) {
                    m_gitCoordinator->requestRefresh();
                });
        connect(controller, &DocumentController::emojiFontInstallationRecommended,
                this, [this](const QString &filePath) {
                    if (m_emojiFontWarningShown)
                        return;

                    m_emojiFontWarningShown = true;
                    auto *message = new QMessageBox(
                        QMessageBox::Information,
                        tr("Emoji Font Not Installed"),
                        tr("%1 contains emoji, but no emoji-capable font was found on this "
                           "system. Install an emoji font such as Noto Color Emoji, "
                           "then restart NotePeek.")
                            .arg(QFileInfo(filePath).fileName()),
                        QMessageBox::Ok,
                        this);
                    message->setAttribute(Qt::WA_DeleteOnClose);
                    message->setModal(false);
                    message->show();
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
    PlatformStyle::current().applyMainWindow(this, ui->editorSplitter);
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
    // Cancel Git while the status bar and file tree still exist: cancellation
    // can synchronously emit stateChanged() and clear their presentation.
    delete m_gitCoordinator;
    m_gitCoordinator = nullptr;
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

    // Unicode first, then ISO-8859-1, separated so the common choices stay on top.
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
        if (!filePath.isEmpty()
            && AppPreferences::instance().revealCurrentFileInTree())
        {
            syncFileExplorerToFile(filePath, false);
        }
    }
    else
    {
        setWindowTitle(tr("NotePeek"));
        setWindowModified(false);
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

bool MainWindow::event(QEvent *event)
{
    const bool handled = QMainWindow::event(event);
    if (event->type() == QEvent::WindowActivate && m_gitCoordinator)
        m_gitCoordinator->handleWindowActivated();
    return handled;
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
    PlatformStyle::current().applyMainWindow(this, ui->editorSplitter);
    ui->statusbar->reapplyTheme();
    ui->widget_findreplace->reapplyTheme();
    m_fileExplorerDock->reapplyTheme();
    m_searchResultsDock->reapplyTheme();
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
    m_gitCoordinator->setDirectory(m_fileExplorerRootDirectory);
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

bool MainWindow::on_action_saveAll_triggered()
{
    QSet<void *> processedDocuments;
    for (DocumentController *controller : std::as_const(m_documentControllers))
    {
        if (controller && !controller->saveAll(&processedDocuments))
            return false;
    }
    return true;
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
    reapplyChromeTheme();
    m_editorCoordinator->refreshVisibleMarkdownPreviews();
    ui->statusbar->setVisible(AppPreferences::instance().showStatusBar());
    m_gitCoordinator->setEnabled(AppPreferences::instance().gitIntegrationEnabled());
    if (AppPreferences::instance().revealCurrentFileInTree())
    {
        if (CustomEditor *editor = activeController()->currentEditor())
        {
            if (!editor->filePath().isEmpty())
                syncFileExplorerToFile(editor->filePath(), false);
        }
    }
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

void MainWindow::on_action_formatCode_triggered()
{
    DocumentController *controller = activeController();
    CustomEditor *editor = controller->currentEditor();
    if (!editor || controller->pane()->isCurrentEditorHidden())
        return;
    m_codeFormattingCoordinator->format(editor);
}

void MainWindow::on_action_switchHeaderSource_triggered()
{
    m_cppNavigation->switchHeaderSource();
}

void MainWindow::on_action_markdownPreview_triggered()
{
    toggleMarkdownSidePreviewFor(activeController());
}

void MainWindow::toggleMarkdownSidePreviewFor(DocumentController *controller)
{
    m_markdownTools->togglePreview(controller);
}

void MainWindow::on_action_markdownExportPdf_triggered()
{
    m_markdownTools->exportToPdf(activeController()->currentEditor());
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

    const bool cppSupported = !filePath.isEmpty() && m_cppNavigation->isCppFile(filePath);
    const bool markdownSupported = !filePath.isEmpty() && m_markdownTools->isMarkdownFile(filePath);

    // A formatter has to exist for the language *and* be usable: C-family files
    // report false while clang-format is not installed, so the entry is never
    // offered when clicking it could only produce an error.
    // A file that is still loading is held read-only, and formatting it would
    // silently do nothing, so availability follows the editor's writability.
    const bool sourceEditable = editor != nullptr
                                && !controller->pane()->isCurrentEditorHidden()
                                && !editor->isReadOnly();
    const bool formatSupported = sourceEditable
                                 && !editor->property("notepeekFormattingInProgress").toBool()
                                 && CodeFormatter::canFormat(filePath);
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
