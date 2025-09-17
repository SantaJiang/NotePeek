#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDebug>
#include <QScreen>
#include <QScroller>
#include <QHeaderView>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include "./editor/filesavedialog.h"
#include "../QScintilla/Qsci/qsciscintilla.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pFileModel(nullptr)
{
    ui->setupUi(this);

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
    ui->widget_findreplace->setMutiEditor(ui->multieditor);

    // 初始化其他组件..
    createDirView();
    createDockWindows();

    m_pDock->setVisible(false);
    ui->widget_findreplace->setVisible(false);

    // 连接MultiEditor的信号到MainWindow的槽..
    connect(ui->multieditor, &MultiEditor::currentEditorChanged, this, [this](QsciScintilla *editor)
    {
        ui->statusbar->connectToEditor(editor);
        refreshHighlights();
    });

    connect(ui->multieditor, &MultiEditor::modificationChanged, this, &MainWindow::updateWindowTitle);
    connect(ui->multieditor, &MultiEditor::copyAvailable, this, &MainWindow::updateEditActions);

    // 连接编辑动作..
    connect(ui->action_cut, &QAction::triggered, this, &MainWindow::on_action_cut_triggered);
    connect(ui->action_copy, &QAction::triggered, this, &MainWindow::on_action_copy_triggered);
    connect(ui->action_paste, &QAction::triggered, this, &MainWindow::on_action_paste_triggered);

    // 初始化编辑动作状态..
    updateEditActions(false);

    // 如果有文件参数，打开文件..
    if (hasFileArg && !filePath.isEmpty())
    {
        openFileByArg(filePath);
    }
}

MainWindow::~MainWindow()
{
    delete m_pFileModel;
    delete ui;
}

void MainWindow::openFileByArg(const QString &fileName)
{
    ui->multieditor->openFileByArg(fileName);

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
        QTimer::singleShot(0, this, [ = ]
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

void MainWindow::on_action_new_triggered()
{
    ui->multieditor->onNewFile();
    setRootDirectory("");
}

void MainWindow::on_action_openFile_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"));
    if (!fileName.isEmpty())
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
                       .arg(QApplication::applicationDisplayName(), QApplication::applicationVersion(), "Copyright © 2025 Santa.", tr("Made for my baby, Yueyue")));
}

void MainWindow::on_action_showSlideBar_triggered(bool checked)
{
    m_pDock->setVisible(checked);
}

void MainWindow::on_action_undo_triggered()
{
    QsciScintilla *editor = ui->multieditor->currentEditor();
    if (editor)
    {
        editor->undo();
    }
}

void MainWindow::on_action_redo_triggered()
{
    QsciScintilla *editor = ui->multieditor->currentEditor();
    if (editor)
    {
        editor->redo();
    }
}

void MainWindow::on_action_cut_triggered()
{
    QsciScintilla *editor = ui->multieditor->currentEditor();
    if (editor)
    {
        editor->cut();
    }
}

void MainWindow::on_action_copy_triggered()
{
    QsciScintilla *editor = ui->multieditor->currentEditor();
    if (editor)
    {
        editor->copy();
    }
}

void MainWindow::on_action_paste_triggered()
{
    QsciScintilla *editor = ui->multieditor->currentEditor();
    if (editor)
    {
        editor->paste();
    }
}

void MainWindow::on_action_find_triggered()
{
    QsciScintilla *editor = ui->multieditor->currentEditor();
    if (!editor) return;

    if(ui->widget_findreplace->isVisible())
        ui->widget_findreplace->find();
    else
        ui->widget_findreplace->show();
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
    ui->action_cut->setEnabled(available);
    ui->action_copy->setEnabled(available);
}

void MainWindow::updateWindowTitle(bool modified)
{
    QsciScintilla *editor = ui->multieditor->currentEditor();
    if (!editor) return;

    QString title;
    QString filePath = editor->property("filePath").toString();

    if (filePath.isEmpty())
    {
        title = tr("Untitled");
    }
    else
    {
        title = QFileInfo(filePath).fileName();
    }

    if (modified)
    {
        setWindowTitle(tr("%1 - %2[*]").arg(tr("NotePeek")).arg(title));
        setWindowModified(true);
    }
    else
    {
        setWindowTitle(tr("%1 - %2").arg(tr("NotePeek")).arg(title));
        setWindowModified(false);
    }
}
