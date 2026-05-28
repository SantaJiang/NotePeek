#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "themecolors.h"
#include <QFileDialog>
#include <QScreen>
#include <QScroller>
#include <QHeaderView>
#include <QFileInfo>
#include <QDir>
#include <QScrollBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <QEvent>
#include <QCoreApplication>
#include <QStyleHints>
#include "./editor/filesavedialog.h"
#include "./editor/customeditor.h"
#include "./decorators/highlightedscrollbardecorator.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pFileModel(nullptr)
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

    // 初始化查找替换部件..
    ui->widget_findreplace->setMultiEditor(ui->multieditor);

    // 设置查找替换部件和管理器的关联
    ui->widget_findreplace->setFindReplaceManager(m_findReplaceManager);

    // 初始化其他组件..
    createDirView();
    createDockWindows();

    m_pDock->setVisible(false);
    ui->widget_findreplace->setVisible(false);

    // 连接MultiEditor的信号到MainWindow的槽..
    connect(ui->multieditor, &MultiEditor::currentEditorChanged, this, [this](CustomEditor *editor)
    {
        ui->statusbar->connectToEditor(editor);

        // 设置当前编辑器到查找管理器
        m_findReplaceManager->setCurrentEditor(editor);

        // 设置查找管理器到当前编辑器的装饰器
        ui->multieditor->setFindReplaceManagerForCurrentEditor(m_findReplaceManager);

        refreshHighlights();
        updateToolActions();
        updateEditActions(editor && editor->hasSelectedText());
    });

    // 连接查找替换部件的可见性变化
    connect(ui->widget_findreplace, &FindReplaceWidget::visibilityChanged, this, [this](bool visible) {
        if (!visible)
        {
            m_findReplaceManager->clearHighlights();
        }
    });

    connect(ui->multieditor, &MultiEditor::modificationChanged, this, &MainWindow::updateWindowTitle);
    connect(ui->multieditor, &MultiEditor::currentFileChanged, this, [this](CustomEditor *, const QString &) {
        updateToolActions();
    });
    connect(ui->multieditor, &MultiEditor::copyAvailable, this, &MainWindow::updateEditActions);
    connect(ui->multieditor, &MultiEditor::filesDropped, this, &MainWindow::onFileDraged);

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
                ui->multieditor->editorManager()->reapplyThemeToAllEditors();
                ui->multieditor->refreshVisibleMarkdownPreview();
            });
#endif

    connect(m_findReplaceManager, &FindReplaceManager::searchResultsChanged, this, &MainWindow::onSearchResultsChanged);
}

MainWindow::~MainWindow()
{
    // m_pFileModel is owned by parent, no need to delete
    delete ui;
}

void MainWindow::openFileByArg(const QString &fileName)
{
    openFileAndSetRootDirectory(fileName);
}

MultiEditor *MainWindow::multiEditor()
{
    return ui->multieditor;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (ui->multieditor->closeAllEditors())
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
        // System theme changed (light ↔ dark). Re-apply theme-aware colours
        // to all open editors and refresh the markdown preview if visible.
        ui->multieditor->editorManager()->reapplyThemeToAllEditors();
        ui->multieditor->refreshVisibleMarkdownPreview();
    }
}

void MainWindow::createDirView()
{
    m_pFileModel = new QFileSystemModel(this);
    m_pFileModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);

    m_pTreeView = new QTreeView(this);
    m_pTreeView->setModel(m_pFileModel);

    m_pTreeView->setAnimated(true);
    m_pTreeView->setIndentation(15);
    m_pTreeView->setSortingEnabled(true);
    m_pTreeView->sortByColumn(0, Qt::AscendingOrder);
    m_pTreeView->setColumnWidth(0, 250);
    m_pTreeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_pTreeView->setHeaderHidden(true);

    m_pTreeView->hideColumn(1);
    m_pTreeView->hideColumn(2);
    m_pTreeView->hideColumn(3);

    connect(m_pTreeView, &QTreeView::doubleClicked, this, &MainWindow::onFileClicked);
    QScroller::grabGesture(m_pTreeView, QScroller::TouchGesture);
}

void MainWindow::createDockWindows()
{
    m_pDock = new QDockWidget(tr("Note Browser"), this);
    m_pDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_pDock->setWidget(m_pTreeView);
    addDockWidget(Qt::LeftDockWidgetArea, m_pDock);
    m_pDock->setTitleBarWidget(new QWidget());
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
    if (!dirPath.isEmpty())
    {
        QModelIndex rootIndex = m_pFileModel->setRootPath(dirPath);
        m_pTreeView->setRootIndex(rootIndex);
        m_pTreeView->expand(rootIndex);
    }
    else
    {
        m_pTreeView->setRootIndex(QModelIndex());
    }
}

void MainWindow::openFileAndSetRootDirectory(const QString &fileName)
{
    ui->multieditor->openFile(fileName);

    QFileInfo fileInfo(fileName);
    QString dirPath = fileInfo.absolutePath();
    setRootDirectory(dirPath);

    QModelIndex index = m_pFileModel->index(fileName);
    if (index.isValid())
    {
        m_pTreeView->scrollTo(index);
        m_pTreeView->setCurrentIndex(index);
    }
}

void MainWindow::on_action_new_triggered()
{
    ui->multieditor->onNewFile();
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
    if (ui->multieditor->maybeSave(ui->multieditor->currentEditor()))
    {
        QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Directory..."), ".");
        if (!dirPath.isEmpty())
        {
            setRootDirectory(dirPath);
            m_pDock->setVisible(true);
            ui->action_showSlideBar->setChecked(true);
            QModelIndex rootIndex = m_pFileModel->index(dirPath);
            if (rootIndex.isValid())
            {
                m_pTreeView->expand(rootIndex);
            }
        }
    }
}

bool MainWindow::on_action_save_triggered()
{
    return ui->multieditor->onSave();
}

bool MainWindow::on_action_saveAs_triggered()
{
    return ui->multieditor->onSaveAs();
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
                       .arg(QApplication::applicationDisplayName(), QApplication::applicationVersion(), "Copyright © 2026 Santa.", tr("Made for my baby, Yueyue")));
}

void MainWindow::on_action_showSlideBar_triggered(bool checked)
{
    m_pDock->setVisible(checked);
}

void MainWindow::on_action_undo_triggered()
{
    if (ui->multieditor->isCurrentEditorPreviewVisible()) return;

    CustomEditor *editor = ui->multieditor->currentEditor();
    if (editor)
    {
        editor->undo();
    }
}

void MainWindow::on_action_redo_triggered()
{
    if (ui->multieditor->isCurrentEditorPreviewVisible()) return;

    CustomEditor *editor = ui->multieditor->currentEditor();
    if (editor)
    {
        editor->redo();
    }
}

void MainWindow::on_action_cut_triggered()
{
    if (ui->multieditor->isCurrentEditorPreviewVisible()) return;

    CustomEditor *editor = ui->multieditor->currentEditor();
    if (editor)
    {
        editor->cut();
    }
}

void MainWindow::on_action_copy_triggered()
{
    ui->multieditor->copyCurrentSelection();
}

void MainWindow::on_action_paste_triggered()
{
    if (ui->multieditor->isCurrentEditorPreviewVisible()) return;

    CustomEditor *editor = ui->multieditor->currentEditor();
    if (editor)
    {
        editor->paste();
    }
}

void MainWindow::on_action_find_triggered()
{
    if (ui->multieditor->isCurrentEditorPreviewVisible()) return;

    CustomEditor *editor = ui->multieditor->currentEditor();
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
    CustomEditor *editor = ui->multieditor->currentEditor();
    if (!editor) return;

    QString currentFilePath = editor->filePath();
    if (currentFilePath.isEmpty()) return;

    // 获取对应的头文件/源文件路径
    QString correspondingFile = m_headerSourceManager->getCorrespondingFile(currentFilePath);

    if (!correspondingFile.isEmpty())
    {
        ui->multieditor->openFile(correspondingFile);

        QModelIndex index = m_pFileModel->index(correspondingFile);
        if (index.isValid()) {
            m_pTreeView->scrollTo(index);
            m_pTreeView->setCurrentIndex(index);
        }
    }
}

void MainWindow::on_action_markdownPreview_triggered()
{
    CustomEditor *editor = ui->multieditor->currentEditor();
    if (!editor) return;

    const QString currentFilePath = editor->filePath();
    if (!m_markdownPreviewer->isMarkdownFile(currentFilePath)) return;

    const bool previewVisible = ui->multieditor->toggleMarkdownPreview(m_markdownPreviewer);
    if (previewVisible)
    {
        ui->widget_findreplace->setVisible(false);
    }

    updateEditActions(editor->hasSelectedText());
    updateToolActions();
}

void MainWindow::on_action_markdownExportPdf_triggered()
{
    CustomEditor *editor = ui->multieditor->currentEditor();
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


void MainWindow::onFileClicked(const QModelIndex &index)
{
    if (!m_pFileModel->isDir(index))
    {
        QString filePath = m_pFileModel->filePath(index);
        ui->multieditor->openFile(filePath);
    }
}

void MainWindow::updateEditActions(bool available)
{
    const bool editingAvailable = !ui->multieditor->isCurrentEditorPreviewVisible();
    ui->action_cut->setEnabled(editingAvailable && available);
    ui->action_copy->setEnabled(available);
    ui->action_paste->setEnabled(editingAvailable);
    ui->action_undo->setEnabled(editingAvailable);
    ui->action_redo->setEnabled(editingAvailable);
    ui->action_find->setEnabled(editingAvailable);
}

void MainWindow::updateWindowTitle(bool modified)
{
    CustomEditor *editor = ui->multieditor->currentEditor();
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
    CustomEditor *editor = ui->multieditor->currentEditor();
    const QString filePath = editor ? editor->filePath() : QString();
    const bool previewVisible = ui->multieditor->isCurrentEditorPreviewVisible();

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
    CustomEditor *editor = ui->multieditor->currentEditor();
    if (editor)
    {
        editor->verticalScrollBar()->update();
    }
}
