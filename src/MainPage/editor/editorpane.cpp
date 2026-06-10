#include "editor/editorpane.h"
#include "ui_editorpane.h"

#include "editor/customeditor.h"
#include "editor/documentpage.h"
#include "editor/editorswitchpopup.h"

#include <QFontDatabase>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTabBar>

EditorPane::EditorPane(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::EditorPane)
    , m_switchPopup(nullptr)
{
    ui->setupUi(this);
    setAcceptDrops(false);

    const int fontId = QFontDatabase::addApplicationFont("://resource/fontawesome-webfont.ttf");
    const QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
    if (!fontFamilies.isEmpty())
    {
        ui->pushButton_closeDoc->setFont(QFont(fontFamilies.first()));
        ui->pushButton_splitView->setFont(QFont(fontFamilies.first()));
    }
    ui->pushButton_closeDoc->setText(QChar(0xf00d));
    ui->pushButton_splitView->setText(QChar(0xf0db));
    ui->pushButton_splitView->setToolTip(tr("Split editor"));

    ui->tabWidget->setTabsClosable(true);
    ui->tabWidget->setDocumentMode(true);
    ui->tabWidget->tabBar()->setVisible(false);

    connect(ui->tabWidget, &QTabWidget::tabCloseRequested, this, &EditorPane::pageCloseRequested);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &EditorPane::onTabCurrentChanged);

    m_switchPopup = new EditorSwitchPopup(this);
    connect(m_switchPopup, &EditorSwitchPopup::itemSelected, this, [this](int index) {
        setCurrentIndex(index);
        if (QWidget *widget = ui->tabWidget->widget(index))
        {
            widget->setFocus();
        }
    });

    // Ctrl+Tab cycles through pages while this pane has focus.
    auto *tabShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab), this);
    tabShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(tabShortcut, &QShortcut::activated, this, &EditorPane::showEditorSwitchPopup);

#ifdef Q_OS_MAC
    // Cmd+Tab is taken by the system on macOS; offer Option+Tab as well.
    auto *tabShortcutMac = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Tab), this);
    tabShortcutMac->setContext(Qt::WidgetWithChildrenShortcut);
    connect(tabShortcutMac, &QShortcut::activated, this, &EditorPane::showEditorSwitchPopup);
#endif
}

EditorPane::~EditorPane()
{
    delete ui;
}

int EditorPane::addPage(DocumentPage *page, const QString &title, const QString &toolTip)
{
    const int index = ui->tabWidget->addTab(page, title);
    ui->tabWidget->setTabToolTip(index, toolTip);

    ui->comboBox_files->addItem(title);
    ui->comboBox_files->setItemData(index, toolTip, Qt::ToolTipRole);
    return index;
}

void EditorPane::removePage(int index)
{
    ui->tabWidget->removeTab(index);
    if (index >= 0 && index < ui->comboBox_files->count())
    {
        ui->comboBox_files->removeItem(index);
    }
}

void EditorPane::replacePage(int index, DocumentPage *page)
{
    const QString tabText = ui->tabWidget->tabText(index);
    const QString tabToolTip = ui->tabWidget->tabToolTip(index);
    const QIcon tabIcon = ui->tabWidget->tabIcon(index);

    const bool tabsWereBlocked = ui->tabWidget->blockSignals(true);
    ui->tabWidget->removeTab(index);
    ui->tabWidget->insertTab(index, page, tabIcon, tabText);
    ui->tabWidget->setTabToolTip(index, tabToolTip);
    ui->tabWidget->setCurrentIndex(index);
    ui->tabWidget->blockSignals(tabsWereBlocked);
}

int EditorPane::count() const
{
    return ui->tabWidget->count();
}

int EditorPane::currentIndex() const
{
    return ui->tabWidget->currentIndex();
}

void EditorPane::setCurrentIndex(int index)
{
    if (index < 0 || index >= ui->tabWidget->count())
        return;

    ui->tabWidget->setCurrentIndex(index);
    syncComboBoxIndex(index);
}

DocumentPage *EditorPane::currentPage() const
{
    return pageForWidget(ui->tabWidget->currentWidget());
}

DocumentPage *EditorPane::pageAt(int index) const
{
    return pageForWidget(ui->tabWidget->widget(index));
}

DocumentPage *EditorPane::pageForEditor(CustomEditor *editor) const
{
    if (!editor)
    {
        return nullptr;
    }

    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        DocumentPage *page = pageForWidget(ui->tabWidget->widget(i));
        if (page && page->editor() == editor)
        {
            return page;
        }
    }

    return nullptr;
}

CustomEditor *EditorPane::currentEditor() const
{
    DocumentPage *page = currentPage();
    return page ? page->editor() : nullptr;
}

CustomEditor *EditorPane::editorAt(int index) const
{
    DocumentPage *page = pageAt(index);
    return page ? page->editor() : nullptr;
}

int EditorPane::indexOfEditor(CustomEditor *editor) const
{
    for (int i = 0; i < ui->tabWidget->count(); ++i)
    {
        DocumentPage *page = pageForWidget(ui->tabWidget->widget(i));
        if (page && page->editor() == editor)
        {
            return i;
        }
    }
    return -1;
}

QString EditorPane::filePathAt(int index) const
{
    CustomEditor *editor = editorAt(index);
    return editor ? editor->filePath() : QString();
}

QString EditorPane::pageTitle(int index) const
{
    return ui->tabWidget->tabText(index);
}

void EditorPane::setPageTitle(int index, const QString &title)
{
    ui->tabWidget->setTabText(index, title);
    if (index >= 0 && index < ui->comboBox_files->count())
    {
        ui->comboBox_files->setItemText(index, title);
    }
}

void EditorPane::setPageToolTip(int index, const QString &toolTip)
{
    ui->tabWidget->setTabToolTip(index, toolTip);
    if (index >= 0 && index < ui->comboBox_files->count())
    {
        ui->comboBox_files->setItemData(index, toolTip, Qt::ToolTipRole);
    }
}

void EditorPane::focusEditor(CustomEditor *editor)
{
    const int index = indexOfEditor(editor);
    if (index == -1)
        return;

    setCurrentIndex(index);
    ui->tabWidget->currentWidget()->setFocus();
}

bool EditorPane::isCurrentEditorPreviewVisible() const
{
    DocumentPage *page = currentPage();
    return page && page->isPreviewVisible();
}

bool EditorPane::toggleMarkdownPreview(MarkdownPreviewer *previewer)
{
    DocumentPage *page = currentPage();
    if (!page || !previewer)
    {
        return false;
    }

    return page->togglePreview(previewer);
}

void EditorPane::hideMarkdownPreview(CustomEditor *editor)
{
    if (DocumentPage *page = pageForEditor(editor))
    {
        page->hidePreview();
    }
}

void EditorPane::refreshVisibleMarkdownPreview()
{
    if (DocumentPage *page = currentPage())
    {
        page->refreshPreview();
    }
}

void EditorPane::copyCurrentSelection()
{
    if (DocumentPage *page = currentPage())
    {
        page->copyCurrentSelection();
    }
}

void EditorPane::setSplitViewButtonChecked(bool checked)
{
    const QSignalBlocker blocker(ui->pushButton_splitView);
    ui->pushButton_splitView->setChecked(checked);
}

void EditorPane::setSplitViewButtonVisible(bool visible)
{
    ui->pushButton_splitView->setVisible(visible);
}

void EditorPane::setTopbarVisible(bool visible)
{
    ui->topbar->setVisible(visible);
}

void EditorPane::on_pushButton_closeDoc_clicked()
{
    const int index = ui->comboBox_files->currentIndex();
    if (index >= 0)
    {
        emit pageCloseRequested(index);
    }
}

void EditorPane::on_pushButton_splitView_clicked(bool checked)
{
    emit splitViewToggled(checked);
}

void EditorPane::on_comboBox_files_currentIndexChanged(int index)
{
    if (index >= 0 && index < ui->tabWidget->count())
    {
        ui->tabWidget->setCurrentIndex(index);
    }
}

void EditorPane::onTabCurrentChanged(int index)
{
    syncComboBoxIndex(index);
    emit currentPageChanged(index);
}

void EditorPane::showEditorSwitchPopup()
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

DocumentPage *EditorPane::pageForWidget(QWidget *widget) const
{
    return qobject_cast<DocumentPage *>(widget);
}

void EditorPane::syncComboBoxIndex(int index)
{
    if (index >= 0 && index < ui->comboBox_files->count())
    {
        const QSignalBlocker blocker(ui->comboBox_files);
        ui->comboBox_files->setCurrentIndex(index);
    }
}
