#include "mainwindow.h"
#include "ui_mainwindow.h"
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
#include "editor/filesavedialog.h"
#include "editor/customeditor.h"
#include "editor/spliteditorcoordinator.h"
#include "decorators/highlightedscrollbardecorator.h"
#include "widgets/fileexplorerdock.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_fileExplorerDock(nullptr)
    , m_editorCoordinator(nullptr)
    , m_singleFileWindow(false)
    , m_findReplaceManager(new FindReplaceManager(this))
    , m_headerSourceManager(new HeaderSourceManager(this))
    , m_markdownPreviewer(new MarkdownPreviewer(this))
{
    ui->setupUi(this);
    setAcceptDrops(false);  // 主窗口不处理拖拽

    // 检查命令行参数..
    QStringList args = QCoreApplication::arguments();
    bool hasFileArg = false;
    QString filePath;
    m_singleFileWindow = args.contains(QStringLiteral("--single-file-window"));

    // 跳过第一个参数（程序自身）..
    for (int i = 1; i < args.size(); ++i)
    {
        if (!args[i].startsWith("-"))   // 假设不以"-"开头的参数是文件路径..
        {
            filePath = args[i];
            hasFileArg = true;
            break;
        }
    }

    // 根据是否有文件参数决定是否创建默认编辑器..
    ui->multieditor->setCreateDefault(!hasFileArg);
    ui->multieditorRight->setCreateDefault(false);

    // 设置查找替换部件和管理器的关联
    ui->widget_findreplace->setFindReplaceManager(m_findReplaceManager);

    m_fileExplorerDock = new FileExplorerDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_fileExplorerDock->dockWidget());
    connect(m_fileExplorerDock, &FileExplorerDock::fileActivated,
            this, [this](const QString &filePath) {
                activeMultiEditor()->openFile(filePath);
            });
    connect(m_fileExplorerDock, &FileExplorerDock::openInNewWindowRequested,
            this, &MainWindow::openFileInNewWindow);

    m_fileExplorerDock->setVisible(false);
    ui->widget_findreplace->setVisible(false);

    ui->editorSplitter->setStretchFactor(0, 1);
    ui->editorSplitter->setStretchFactor(1, 1);
    m_editorCoordinator = new SplitEditorCoordinator(ui->multieditor,
                                                     ui->multieditorRight,
                                                     ui->editorSplitter,
                                                     m_singleFileWindow,
                                                     this);
    connect(m_editorCoordinator, &SplitEditorCoordinator::activeMultiEditorChanged,
            this, &MainWindow::applyActiveMultiEditor);
    connect(m_editorCoordinator, &SplitEditorCoordinator::paneModificationChanged,
            this, [this](MultiEditor *multiEditor, bool modified) {
                if (multiEditor == activeMultiEditor())
                {
                    updateWindowTitle(modified);
                }
            });
    connect(m_editorCoordinator, &SplitEditorCoordinator::paneCopyAvailableChanged,
            this, [this](MultiEditor *multiEditor, bool available) {
                if (multiEditor == activeMultiEditor())
                {
                    updateEditActions(available);
                }
            });
    connect(m_editorCoordinator, &SplitEditorCoordinator::filesDropped,
            this, [this](MultiEditor *, const QList<QUrl> &urls) {
                onFileDraged(urls);
            });
    applyActiveMultiEditor(ui->multieditor);
    if (m_singleFileWindow)
    {
        ui->multieditor->setTopbarVisible(false);
        ui->multieditorRight->setTopbarVisible(false);
    }

    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        if (!now)
            return;

        m_editorCoordinator->updateActivePaneForFocus(now);
    });

    // 连接查找替换部件的可见性变化
    connect(ui->widget_findreplace, &FindReplaceWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible)
        {
            m_findReplaceManager->clearHighlights();
        }
    });

    // 连接编辑动作..
    connect(ui->action_cut, &QAction::triggered, this, &MainWindow::on_action_cut_triggered);
    connect(ui->action_copy, &QAction::triggered, this, &MainWindow::on_action_copy_triggered);
    connect(ui->action_paste, &QAction::triggered, this, &MainWindow::on_action_paste_triggered);

    // 初始化编辑动作状态..
    updateEditActions(false);
    updateToolActions();

    // 如果有文件参数，打开文件..
    if (hasFileArg && !filePath.isEmpty())
    {
        openFileByArg(filePath);
    }

    // Listen for system theme changes (light ↔ dark) at runtime.
    // QEvent::PaletteChange alone is unreliable on macOS with the Fusion style,
    // so we also connect to the dedicated colorSchemeChanged signal (Qt 6.5+).
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, [this]() {
                m_editorCoordinator->reapplyThemeToEditorPanes();
                m_editorCoordinator->refreshVisibleMarkdownPreviews();
            });
#endif

    connect(m_findReplaceManager, &FindReplaceManager::searchResultsChanged, this, &MainWindow::onSearchResultsChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openFileByArg(const QString &fileName)
{
    openFileAndSetRootDirectory(fileName);
}

MultiEditor *MainWindow::multiEditor()
{
    return activeMultiEditor();
}

MultiEditor *MainWindow::activeMultiEditor() const
{
    return m_editorCoordinator ? m_editorCoordinator->activeMultiEditor() : ui->multieditor;
}

void MainWindow::applyActiveMultiEditor(MultiEditor *multiEditor)
{
    if (!multiEditor)
        return;

    ui->widget_findreplace->setMultiEditor(multiEditor);

    CustomEditor *editor = multiEditor->currentEditor();
    ui->statusbar->connectToEditor(editor);
    m_findReplaceManager->setCurrentEditor(editor);
    multiEditor->setFindReplaceManagerForCurrentEditor(m_findReplaceManager);

    updateToolActions();
    updateEditActions(editor && editor->hasSelectedText());
    if (editor)
    {
        updateWindowTitle(editor->isModified());
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

void MainWindow::setRootDirectory(const QString &dirPath)
{
    m_fileExplorerDock->setRootDirectory(dirPath);
}

void MainWindow::openFileAndSetRootDirectory(const QString &fileName)
{
    activeMultiEditor()->openFile(fileName);

    QFileInfo fileInfo(fileName);
    QString dirPath = fileInfo.absolutePath();
    setRootDirectory(dirPath);
    m_fileExplorerDock->selectFile(fileName);
}

void MainWindow::on_action_new_triggered()
{
    activeMultiEditor()->onNewFile();
    setRootDirectory(QString());
}

void MainWindow::on_action_openFile_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"));
    if (!fileName.isEmpty())
    {
        openFileAndSetRootDirectory(fileName);
    }
}

void MainWindow::on_action_openDirectory_triggered()
{
    MultiEditor *multiEditor = activeMultiEditor();
    if (multiEditor->maybeSave(multiEditor->currentEditor()))
    {
        QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Directory..."), ".");
        if (!dirPath.isEmpty())
        {
            setRootDirectory(dirPath);
            m_fileExplorerDock->setVisible(true);
            ui->action_showSlideBar->setChecked(true);
            m_fileExplorerDock->expandDirectory(dirPath);
        }
    }
}

bool MainWindow::on_action_save_triggered()
{
    return activeMultiEditor()->onSave();
}

bool MainWindow::on_action_saveAs_triggered()
{
    return activeMultiEditor()->onSaveAs();
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

void MainWindow::on_action_undo_triggered()
{
    MultiEditor *multiEditor = activeMultiEditor();
    if (multiEditor->isCurrentEditorPreviewVisible()) return;

    CustomEditor *editor = multiEditor->currentEditor();
    if (editor)
    {
        editor->undo();
    }
}

void MainWindow::on_action_redo_triggered()
{
    MultiEditor *multiEditor = activeMultiEditor();
    if (multiEditor->isCurrentEditorPreviewVisible()) return;

    CustomEditor *editor = multiEditor->currentEditor();
    if (editor)
    {
        editor->redo();
    }
}

void MainWindow::on_action_cut_triggered()
{
    MultiEditor *multiEditor = activeMultiEditor();
    if (multiEditor->isCurrentEditorPreviewVisible()) return;

    CustomEditor *editor = multiEditor->currentEditor();
    if (editor)
    {
        editor->cut();
    }
}

void MainWindow::on_action_copy_triggered()
{
    activeMultiEditor()->copyCurrentSelection();
}

void MainWindow::on_action_paste_triggered()
{
    MultiEditor *multiEditor = activeMultiEditor();
    if (multiEditor->isCurrentEditorPreviewVisible()) return;

    CustomEditor *editor = multiEditor->currentEditor();
    if (editor)
    {
        editor->paste();
    }
}

void MainWindow::on_action_find_triggered()
{
    MultiEditor *multiEditor = activeMultiEditor();
    if (multiEditor->isCurrentEditorPreviewVisible()) return;

    CustomEditor *editor = multiEditor->currentEditor();
    if (!editor) return;

    QString selectedText = editor->selectedText();
    if (ui->widget_findreplace->isVisible())
    {
        ui->widget_findreplace->setFocus();
    }
    else
    {
        ui->widget_findreplace->showWithInitialText(selectedText);
    }
}

void MainWindow::on_action_switchHeaderSource_triggered()
{
    MultiEditor *multiEditor = activeMultiEditor();
    CustomEditor *editor = multiEditor->currentEditor();
    if (!editor) return;

    QString currentFilePath = editor->filePath();
    if (currentFilePath.isEmpty()) return;

    // 获取对应的头文件/源文件路径
    QString correspondingFile = m_headerSourceManager->getCorrespondingFile(currentFilePath);

    if (!correspondingFile.isEmpty())
    {
        multiEditor->openFile(correspondingFile);
        m_fileExplorerDock->selectFile(correspondingFile);
    }
}

void MainWindow::on_action_markdownPreview_triggered()
{
    MultiEditor *multiEditor = activeMultiEditor();
    CustomEditor *editor = multiEditor->currentEditor();
    if (!editor) return;

    const QString currentFilePath = editor->filePath();
    if (!m_markdownPreviewer->isMarkdownFile(currentFilePath)) return;

    const bool previewVisible = multiEditor->toggleMarkdownPreview(m_markdownPreviewer);
    if (previewVisible)
    {
        ui->widget_findreplace->setVisible(false);
    }

    updateEditActions(editor->hasSelectedText());
    updateToolActions();
}

void MainWindow::on_action_markdownExportPdf_triggered()
{
    CustomEditor *editor = activeMultiEditor()->currentEditor();
    if (!editor) return;

    const QString currentFilePath = editor->filePath();
    if (!m_markdownPreviewer->isMarkdownFile(currentFilePath)) return;

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
            QString filePath = url.toLocalFile();
            openFileByArg(filePath);
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
    const bool editingAvailable = !activeMultiEditor()->isCurrentEditorPreviewVisible();
    ui->action_cut->setEnabled(editingAvailable && available);
    ui->action_copy->setEnabled(available);
    ui->action_paste->setEnabled(editingAvailable);
    ui->action_undo->setEnabled(editingAvailable);
    ui->action_redo->setEnabled(editingAvailable);
    ui->action_find->setEnabled(editingAvailable);
}

void MainWindow::updateWindowTitle(bool modified)
{
    CustomEditor *editor = activeMultiEditor()->currentEditor();
    if (!editor) return;

    QString title;
    QString filePath = editor->filePath();

    if (filePath.isEmpty())
    {
        title = tr("Untitled");
    }
    else
    {
        title = QFileInfo(filePath).fileName();
    }

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
    MultiEditor *multiEditor = activeMultiEditor();
    CustomEditor *editor = multiEditor->currentEditor();
    const QString filePath = editor ? editor->filePath() : QString();
    const bool previewVisible = multiEditor->isCurrentEditorPreviewVisible();

    const bool cppSupported = !filePath.isEmpty() && m_headerSourceManager->isCppFile(filePath);
    const bool markdownSupported = !filePath.isEmpty() && m_markdownPreviewer->isMarkdownFile(filePath);

    ui->action_switchHeaderSource->setEnabled(cppSupported && !previewVisible);
    ui->menuToolCpp->setEnabled(cppSupported && !previewVisible);
    ui->action_markdownPreview->setEnabled(markdownSupported);
    ui->action_markdownPreview->setChecked(markdownSupported && previewVisible);
    ui->action_markdownPreview->setText(previewVisible ? tr("Show Markdown Source") : tr("Preview Markdown"));
    ui->action_markdownExportPdf->setEnabled(markdownSupported);
    ui->menuToolMarkdown->setEnabled(markdownSupported);
    ui->menuTool->setEnabled(true);
}

void MainWindow::onSearchResultsChanged()
{
    CustomEditor *editor = activeMultiEditor()->currentEditor();
    if (editor)
    {
        editor->verticalScrollBar()->update();
    }
}
