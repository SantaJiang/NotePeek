#include "multieditor.h"
#include "ui_multieditor.h"
#include "filesavedialog.h"
#include "lexerfactory.h"
#include "editorswitchpopup.h"
#include "../find/findreplacemanager.h"
#include <QTabBar>
#include <QShortcut>
#include <QFileDialog>
#include <QFontDatabase>
#include <QApplication>
#include <QFileInfo>
#include <QTextStream>
#include <QMap>
#include <QDebug>
#include <QMessageBox>
#include <QCoreApplication>
#include <QRegularExpression>
#include "customeditor.h"
#include "../decorators/highlightedscrollbardecorator.h"

MultiEditor::MultiEditor(QWidget *parent, bool createDefault) : QWidget(parent),
    ui(new Ui::MultiEditor),
    m_defaultEditor(nullptr),
    m_switchPopup(nullptr)  // 初始化弹出窗口指针
{
    ui->setupUi(this);
    setAcceptDrops(false);

    int fontId = QFontDatabase::addApplicationFont("://resource/fontawesome-webfont.ttf");
    QString fontName = QFontDatabase::applicationFontFamilies(fontId).at(0);
    QFont iconFont = QFont(fontName);
    ui->pushButton_closeDoc->setFont(iconFont);
    ui->pushButton_closeDoc->setText(QChar(0xf00d));

    // 初始化编辑器管理器..
    m_editorManager = new EditorManager(this);
    connect(m_editorManager, &EditorManager::filesDropped, this, &MultiEditor::filesDropped);

    // 设置标签页..
    ui->tabWidget->setTabsClosable(true);
    ui->tabWidget->setDocumentMode(true);
    ui->tabWidget->tabBar()->setVisible(false);

    // 连接信号槽..
    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &MultiEditor::tabCloseRequested);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MultiEditor::currentTabChanged);

    // 连接标签页变化到下拉框更新..
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &MultiEditor::setCurrentTabIndex);

    // 只有在需要时才创建默认编辑器..
    if (createDefault)
    {
        createDefaultDoc();
    }

    // 创建切换弹出窗口
    m_switchPopup = new EditorSwitchPopup(this);
    connect(m_switchPopup, &EditorSwitchPopup::itemSelected, this, &MultiEditor::switchToEditorByIndex);
    connect(m_switchPopup, &EditorSwitchPopup::popupClosed, this, &MultiEditor::onSwitchPopupClosed);

    // Ctrl+Tab 切换编辑器（仅在本控件及子控件获得焦点时生效）
    auto *tabShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab), this);
    tabShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(tabShortcut, &QShortcut::activated, this, &MultiEditor::showEditorSwitchPopup);

#ifdef Q_OS_MAC
    // macOS 上 Cmd+Tab 被系统占用，改用 Option+Tab
    auto *tabShortcutMac = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Tab), this);
    tabShortcutMac->setContext(Qt::WidgetWithChildrenShortcut);
    connect(tabShortcutMac, &QShortcut::activated, this, &MultiEditor::showEditorSwitchPopup);
#endif
}

MultiEditor::~MultiEditor()
{
    delete ui;
}

CustomEditor *MultiEditor::currentEditor() const
{
    if(ui->tabWidget == nullptr)
    {
        return nullptr;
    }

    return qobject_cast<CustomEditor *>(ui->tabWidget->currentWidget());
}

int MultiEditor::findEditorIndex(CustomEditor *editor) const
{
    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        if (ui->tabWidget->widget(i) == editor)
        {
            return i;
        }
    }
    return -1;
}

void MultiEditor::switchToEditor(CustomEditor *editor)
{
    if (!editor) return;

    // 查找编辑器在tabWidget中的索引..
    int index = findEditorIndex(editor);
    if (index != -1)
    {
        // 切换到对应的标签页..
        ui->tabWidget->setCurrentIndex(index);
        // 确保编辑器获得焦点..
        editor->setFocus();
    }
}

bool MultiEditor::maybeSave(CustomEditor *editor)
{
    if (!editor || !editor->isModified()) return true;

    switchToEditor(editor);

    // 使用新的多文件对话框，但只显示当前文件..
    QMap<CustomEditor*, QString> files;
    QString filePath = editor->filePath();
    QString displayName = filePath.isEmpty() ? tr("untitled.txt") : QFileInfo(filePath).fileName();
    files.insert(editor, displayName);

    FileSaveDialog dialog(files, this);
    int result = dialog.exec();

    if (result == QDialog::Accepted)
    {
        QString filePath = editor->filePath();
        if (filePath.isEmpty())
        {
            return onSaveAs();
        }
        else
        {
            return saveFile(editor, filePath);
        }
    }
    else if (result == FileSaveDialog::DontSave)
    {
        return true;
    }

    return false;
}

bool MultiEditor::saveFile(CustomEditor *editor, const QString &filePath)
{
    if (!editor) return false;

    QFile file(filePath);
    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        QMessageBox::warning(this, tr("NotePeek"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(filePath)
                             .arg(file.errorString()));
        return false;
    }

    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif

    QApplication::setOverrideCursor(Qt::WaitCursor);
    out << editor->text();
    QApplication::restoreOverrideCursor();

    setCurrentFile(editor, filePath);
    m_editorManager->updateEditorSyntax(editor, filePath);
    return true;
}

void MultiEditor::openFile(const QString &filePath)
{
    qDebug()<<Q_FUNC_INFO<<filePath;
    // 检查是否有默认空白编辑器且未修改..
    if (m_defaultEditor && !m_defaultEditor->isModified() && m_defaultEditor->text().isEmpty())
    {
        int index = findEditorIndex(m_defaultEditor);
        if (index != -1)
        {
            // 关闭空白编辑器，不提醒..
            ui->tabWidget->removeTab(index);
            removeTab(index);
            emit editorClosed(m_defaultEditor);
            m_defaultEditor->deleteLater();
            m_defaultEditor = nullptr;
        }
    }

    // Check if file is already open
    CustomEditor *existingEditor = m_editorManager->getEditorByFilePath(filePath);
    if (existingEditor)
    {
        int index = findEditorIndex(existingEditor);
        if (index != -1)
        {
            ui->tabWidget->setCurrentIndex(index);
            setCurrentTabIndex(index); // 确保下拉框也同步..
        }
        return;
    }

    // Create new editor
    CustomEditor *editor = m_editorManager->createEditorFromFile(filePath, false);
    if (!editor)
    {
        return;
    }

    // Add to tab widget
    QFileInfo fileInfo(filePath);
    int tabIndex = ui->tabWidget->addTab(editor, fileInfo.fileName());
    ui->tabWidget->setCurrentIndex(tabIndex);

    // 添加到下拉框..
    addTab(fileInfo.fileName(), filePath);

    // 确保下拉框当前项同步..
    setCurrentTabIndex(tabIndex);

    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox::warning(this, tr("NotePeek"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(filePath)
                             .arg(file.errorString()));
        return;
    }

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif

    QApplication::setOverrideCursor(Qt::WaitCursor);
    editor->setText(in.readAll());
    QApplication::restoreOverrideCursor();

    setCurrentFile(editor, filePath);
    m_editorManager->updateLineNumberMarginWidth(editor);

    // 发出文件打开信号..
    emit fileOpened(editor, filePath);

    // 连接编辑器信号..
    connectEditorSignals(editor);
}

void MultiEditor::setCurrentFile(CustomEditor *editor, const QString &filePath)
{
    if (!editor)
        return;

    editor->setFilePath(filePath);
    editor->setModified(false);

    QString shownName;
    if (filePath.isEmpty())
        shownName = "untitled.txt";
    else
        shownName = QFileInfo(filePath).fileName();

    int index = findEditorIndex(editor);
    if (index != -1)
    {
        ui->tabWidget->setTabText(index, shownName);
        ui->tabWidget->setTabToolTip(index, filePath);

        // 更新下拉框中的显示..
        setTabText(index, shownName);
        setTabToolTip(index, filePath);
    }

    emit modificationChanged(false);
}

void MultiEditor::createDefaultDoc()
{
    CustomEditor *editor = m_editorManager->createEditor(tr("Untitled"));
    int tabIndex = ui->tabWidget->addTab(editor, tr("Untitled"));
    ui->tabWidget->setCurrentIndex(tabIndex);

    // 设置为默认编辑器..
    m_defaultEditor = editor;

    // 添加到下拉框..
    addTab(tr("Untitled"));
    setCurrentTabIndex(tabIndex);

    // 连接编辑器信号..
    connectEditorSignals(editor);

    // 发出编辑器创建信号..
    emit editorCreated(editor);
}

void MultiEditor::onNewFile()
{
    if (maybeSave(currentEditor()))
    {
        createDefaultDoc();
    }
}

void MultiEditor::onOpenFile(const QString &fileName)
{
    QString filePath = fileName;
    if (filePath.isEmpty())
    {
        filePath = QFileDialog::getOpenFileName(this, tr("Open File"));
    }

    if (!filePath.isEmpty())
    {
        openFile(filePath);
    }
}

bool MultiEditor::onSave()
{
    CustomEditor *editor = currentEditor();
    if (!editor) return false;

    QString filePath = editor->filePath();
    if (filePath.isEmpty())
    {
        return onSaveAs();
    }
    else
    {
        return saveFile(editor, filePath);
    }
}

bool MultiEditor::onSaveAs()
{
    CustomEditor *editor = currentEditor();
    if (!editor) return false;

    QString fileName = promptSaveFileName(this);
    if (fileName.isEmpty())
        return false;

    return saveFile(editor, fileName);
}

QString MultiEditor::promptSaveFileName(QWidget *parent)
{
    QStringList filters;
    filters.append(tr("Text Files (*.txt)"));
    for (const QString &ext : LexerFactory::supportedExtensions())
        filters.append(QStringLiteral("%1 (*.%2)").arg(LexerFactory::languageName(LexerFactory::languageFromExtension(ext)), ext));
    filters.append(tr("All Files (*)"));
    QString selectedFilter;

    QString fileName = QFileDialog::getSaveFileName(parent, tr("Save As"), QStringLiteral("untitled.txt"),
                                                    filters.join(QStringLiteral(";;")), &selectedFilter);
    if (fileName.isEmpty())
        return {};

    if (QFileInfo(fileName).suffix().isEmpty()
        && !selectedFilter.isEmpty()
        && selectedFilter != tr("All Files (*)"))
    {
        QRegularExpression re(QStringLiteral("\\(\\*\\.([^)]+)\\)"));
        QRegularExpressionMatch match = re.match(selectedFilter);
        if (match.hasMatch())
        {
            const QString ext = match.captured(1);
            if (!ext.isEmpty())
                fileName += u'.' + ext;
        }
    }
    return fileName;
}

void MultiEditor::addTab(const QString &text, const QString &filePath)
{
    ui->comboBox_files->addItem(text);
    int index = ui->comboBox_files->count() - 1;
    ui->comboBox_files->setItemData(index, filePath, Qt::ToolTipRole);
}

void MultiEditor::removeTab(int index)
{
    if (index >= 0 && index < ui->comboBox_files->count())
    {
        ui->comboBox_files->removeItem(index);
    }
}

void MultiEditor::setTabText(int index, const QString &text)
{
    if (index >= 0 && index < ui->comboBox_files->count())
    {
        ui->comboBox_files->setItemText(index, text);
    }
}

void MultiEditor::setTabToolTip(int index, const QString &toolTip)
{
    if (index >= 0 && index < ui->comboBox_files->count())
    {
        ui->comboBox_files->setItemData(index, toolTip, Qt::ToolTipRole);
    }
}

void MultiEditor::setCurrentTabIndex(int index)
{
    if (index >= 0 && index < ui->comboBox_files->count())
    {
        // 阻塞信号，避免触发currentIndexChanged信号..
        ui->comboBox_files->blockSignals(true);
        ui->comboBox_files->setCurrentIndex(index);
        ui->comboBox_files->blockSignals(false);
    }
}

int MultiEditor::count() const
{
    return ui->comboBox_files->count();
}

void MultiEditor::clear()
{
    ui->comboBox_files->clear();
}

void MultiEditor::tabCloseRequested(int index)
{
    CustomEditor *editor = qobject_cast<CustomEditor *>(ui->tabWidget->widget(index));
    if (!editor) return;

    // 如果关闭的是默认编辑器，清除指针..
    if (editor == m_defaultEditor)
    {
        m_defaultEditor = nullptr;
    }

    if (maybeSave(editor))
    {
        ui->tabWidget->removeTab(index);
        // 从下拉框中移除对应项..
        removeTab(index);

        // 发出编辑器关闭信号..
        emit editorClosed(editor);

        editor->deleteLater();

        // If no tabs left, create a new empty editor
        if (ui->tabWidget->count() == 0)
        {
            createDefaultDoc();
        }
    }
}

void MultiEditor::currentTabChanged(int index)
{
    if (index == -1) return;

    CustomEditor *editor = qobject_cast<CustomEditor *>(ui->tabWidget->widget(index));
    if (!editor) return;

    // 发出当前编辑器改变信号..
    emit currentEditorChanged(editor);

    // 更新下拉框当前项..
    setCurrentTabIndex(index);

    // Update window title
    QString title = ui->tabWidget->tabText(index);
    if (title.endsWith('*'))
    {
        title = title.left(title.length() - 1);
        emit modificationChanged(true);
    }
    else
    {
        emit modificationChanged(false);
    }

    // Update edit actions
    emit copyAvailable(editor->hasSelectedText());
}

void MultiEditor::documentWasModified()
{
    CustomEditor *editor = qobject_cast<CustomEditor *>(sender());
    if (!editor) return;

    // 如果默认编辑器被修改，取消其默认状态
    if (editor == m_defaultEditor && editor->isModified())
    {
        m_defaultEditor = nullptr;
    }

    int index = findEditorIndex(editor);
    if (index == -1) return;

    QString title = ui->tabWidget->tabText(index);
    if (title.endsWith('*'))
    {
        title = title.left(title.length() - 1);
    }

    if (editor->isModified())
    {
        ui->tabWidget->setTabText(index, title + '*');
        setTabText(index, title + '*');
        emit modificationChanged(true);
    }
    else
    {
        ui->tabWidget->setTabText(index, title);
        setTabText(index, title);
        emit modificationChanged(false);
    }

    // 发出编辑器修改状态改变信号..
    emit editorModificationChanged(editor, editor->isModified());
}

void MultiEditor::on_pushButton_closeDoc_clicked()
{
    int currentIndex = ui->comboBox_files->currentIndex();
    if (currentIndex >= 0)
    {
        tabCloseRequested(currentIndex);
    }
}

void MultiEditor::on_comboBox_files_currentIndexChanged(int index)
{
    if(index >= 0 && index < ui->tabWidget->count())
    {
        ui->tabWidget->setCurrentIndex(index);
    }
}

void MultiEditor::connectEditorSignals(CustomEditor *editor)
{
    if (!editor) return;

    connect(editor, SIGNAL(modificationChanged(bool)), this, SLOT(documentWasModified()));
    connect(editor, SIGNAL(copyAvailable(bool)), this, SIGNAL(copyAvailable(bool)));
}

bool MultiEditor::closeAllEditors()
{
    // 收集所有未保存的文件..
    QMap<CustomEditor*, QString> modifiedFiles;
    QList<CustomEditor *> listEditors = m_editorManager->getEditors();
    for(CustomEditor *editor : listEditors)
    {
        if (editor && editor->isModified())
        {
            QString filePath = editor->filePath();
            QString displayName = filePath.isEmpty() ? tr("untitled.txt") : QFileInfo(filePath).fileName();
            modifiedFiles.insert(editor, displayName);
        }
    }

    if (modifiedFiles.isEmpty())
    {
        return true;
    }

    // 显示自定义保存对话框..
    FileSaveDialog dialog(modifiedFiles, this->parentWidget());
    int result = dialog.exec();

    if (result == QDialog::Accepted)
    {
        // 保存选中的文件..
        QList<CustomEditor*> editorsToSave = dialog.selectedEditors();
        bool allSaved = true;

        for(CustomEditor *editor : editorsToSave)
        {
            QString filePath = editor->filePath();
            if (filePath.isEmpty())
            {
                filePath = promptSaveFileName(this->parentWidget());
                if (filePath.isEmpty())
                {
                    allSaved = false;
                    break;
                }
            }

            if (!saveFile(editor, filePath))
            {
                allSaved = false;
                break;
            }
        }

        return allSaved;
    }
    else if (result == FileSaveDialog::DontSave)
    {
        return true;
    }

    return false;
}

void MultiEditor::setCreateDefault(bool createDefault)
{
    if (!createDefault && m_defaultEditor)
    {
        // 如果不创建默认编辑器且当前有默认编辑器，则关闭它..
        int index = findEditorIndex(m_defaultEditor);
        if (index != -1)
        {
            ui->tabWidget->removeTab(index);
            removeTab(index);
            emit editorClosed(m_defaultEditor);
            m_defaultEditor->deleteLater();
            m_defaultEditor = nullptr;
        }
    }
    else if (createDefault && !m_defaultEditor && ui->tabWidget->count() == 0)
    {
        // 如果需要创建默认编辑器且当前没有编辑器，则创建..
        createDefaultDoc();
    }
}

void MultiEditor::setFindReplaceManagerForCurrentEditor(FindReplaceManager *manager)
{
    CustomEditor *editor = currentEditor();
    if (!editor || !manager) return;

    manager->setCurrentEditor(editor);

    HighlightedScrollBarDecorator *decorator = editor->findChild<HighlightedScrollBarDecorator*>();
    if (decorator)
    {
        connect(manager, &FindReplaceManager::searchResultsChanged,
                decorator, &HighlightedScrollBarDecorator::updateScrollBar,
                Qt::UniqueConnection);
    }
}

void MultiEditor::showEditorSwitchPopup()
{
    if (m_switchPopup && m_switchPopup->isVisible())
        return;

    if (ui->tabWidget->count() <= 1)
        return;

    QStringList items;
    QStringList toolTips;

    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        items.append(ui->tabWidget->tabText(i).remove('*'));
        toolTips.append(ui->tabWidget->tabToolTip(i));
    }

    m_switchPopup->setItems(items, toolTips);
    m_switchPopup->setCurrentIndex(ui->tabWidget->currentIndex());
    m_switchPopup->showPopup();
}

void MultiEditor::switchToEditorByIndex(int index)
{
    if (index >= 0 && index < ui->tabWidget->count())
    {
        ui->tabWidget->setCurrentIndex(index);
        // 确保编辑器获得焦点
        CustomEditor *editor = qobject_cast<CustomEditor *>(ui->tabWidget->widget(index));
        if (editor)
        {
            editor->setFocus();
        }
    }
}

void MultiEditor::onSwitchPopupClosed()
{
    // 弹出窗口关闭后的清理工作（如果需要）
}
