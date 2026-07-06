#include "editor/editorpane.h"
#include "ui_editorpane.h"

#include "editor/customeditor.h"
#include "editor/documentpage.h"
#include "editor/editorswitchpopup.h"
#include "themecolors.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPainter>
#include <QPointer>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTabBar>

namespace {

class FileComboPopupDelegate final : public QStyledItemDelegate
{
public:
    explicit FileComboPopupDelegate(QComboBox *comboBox)
        : QStyledItemDelegate(comboBox)
        , m_comboBox(comboBox)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        using namespace ThemeColors::EditorTopBar;
        const bool current = m_comboBox && index.row() == m_comboBox->currentIndex();
        const bool highlighted = opt.state.testFlag(QStyle::State_MouseOver);

        painter->save();
        painter->fillRect(opt.rect, highlighted ? hoverBg() : popupBg());

        const int checkColumnWidth = 28;
        const QRect checkRect(opt.rect.left() + 10, opt.rect.top(), 10, opt.rect.height());
        const QRect textRect = opt.rect.adjusted(checkColumnWidth, 0, -8, 0);

        if (current)
        {
            QPen checkPen(text());
            checkPen.setWidthF(1.6);
            checkPen.setCapStyle(Qt::RoundCap);
            checkPen.setJoinStyle(Qt::RoundJoin);
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(checkPen);

            const int centerY = checkRect.center().y();
            const QPoint p1(checkRect.left(), centerY);
            const QPoint p2(checkRect.left() + 4, centerY + 4);
            const QPoint p3(checkRect.right(), centerY - 5);
            painter->drawLine(p1, p2);
            painter->drawLine(p2, p3);
        }

        painter->setPen(text());
        painter->setFont(opt.font);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, opt.text);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), 28));
        return size;
    }

private:
    QPointer<QComboBox> m_comboBox;
};

} // namespace

EditorPane::EditorPane(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::EditorPane)
    , m_switchPopup(nullptr)
{
    ui->setupUi(this);
    setAcceptDrops(false);

    ui->topbar->setAttribute(Qt::WA_StyledBackground, true);
    installTopbarSeparators();
    ui->comboBox_files->setFrame(false);
    ui->comboBox_files->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    ui->comboBox_files->setItemDelegate(new FileComboPopupDelegate(ui->comboBox_files));
    applyChromeTheme();

    const int fontId = QFontDatabase::addApplicationFont("://resource/fontawesome-webfont.ttf");
    const QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
    if (!fontFamilies.isEmpty())
    {
        ui->pushButton_closeDoc->setFont(QFont(fontFamilies.first()));
        ui->pushButton_splitView->setFont(QFont(fontFamilies.first()));
    }
    ui->pushButton_closeDoc->setText(QChar(0xf00d));
    setSplitViewButtonCollapseMode(false);

    ui->tabWidget->setTabsClosable(true);
    ui->tabWidget->setDocumentMode(true);
    ui->tabWidget->tabBar()->setVisible(false);
    ui->tabWidget->setStyleSheet(QStringLiteral(
        "QTabWidget#tabWidget::pane {"
        "  border: 0;"
        "  margin: 0;"
        "  padding: 0;"
        "}"));

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

bool EditorPane::isCurrentEditorHidden() const
{
    DocumentPage *page = currentPage();
    return page && page->isEditorHidden();
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

void EditorPane::setSplitViewButtonCollapseMode(bool collapseMode)
{
    const QSignalBlocker blocker(ui->pushButton_splitView);
    ui->pushButton_splitView->setChecked(false);
    ui->pushButton_splitView->setIcon(QIcon());
    ui->pushButton_splitView->setText(collapseMode ? QChar(0xf147) : QChar(0xf0db));
    ui->pushButton_splitView->setToolTip(collapseMode
                                             ? tr("Collapse this editor pane")
                                             : tr("Split editor"));
}

void EditorPane::applyChromeTheme()
{
    applyTopbarStyleSheet();
}

void EditorPane::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);

    if (event->type() == QEvent::PaletteChange)
    {
        applyChromeTheme();
    }
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

void EditorPane::installTopbarSeparators()
{
    auto *layout = qobject_cast<QHBoxLayout *>(ui->topbar->layout());
    if (!layout || ui->topbar->findChild<QFrame *>(QStringLiteral("editorTopbarSeparator")))
    {
        return;
    }

    const auto createSeparator = [this]() {
        auto *separator = new QFrame(ui->topbar);
        separator->setObjectName(QStringLiteral("editorTopbarSeparator"));
        separator->setProperty("editorTopbarSeparator", true);
        separator->setFixedWidth(1);
        separator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        return separator;
    };

    layout->insertWidget(layout->indexOf(ui->comboBox_files) + 1, createSeparator());
    layout->insertWidget(layout->indexOf(ui->pushButton_closeDoc) + 1, createSeparator());
    layout->insertWidget(layout->indexOf(ui->pushButton_splitView), createSeparator());
}

void EditorPane::applyTopbarStyleSheet()
{
    using namespace ThemeColors::EditorTopBar;

    const QString topbarStyle = QStringLiteral(
        "QWidget#topbar {"
        "  background-color: %1;"
        "  border-top: 1px solid %2;"
        "  border-bottom: 1px solid %2;"
        "}"
        "QFrame#editorTopbarSeparator {"
        "  background-color: %2;"
        "  border: 0;"
        "  margin: 0;"
        "}"
        "QComboBox#comboBox_files {"
        "  background-color: transparent;"
        "  color: %3;"
        "  border: 0;"
        "  padding: 0 22px 0 8px;"
        "}"
        "QComboBox#comboBox_files:hover {"
        "  background-color: %4;"
        "}"
        "QComboBox#comboBox_files::drop-down {"
        "  subcontrol-origin: padding;"
        "  subcontrol-position: top right;"
        "  width: 22px;"
        "  border: 0;"
        "}"
        "QToolButton#pushButton_closeDoc,"
        "QToolButton#pushButton_splitView {"
        "  background-color: transparent;"
        "  color: %6;"
        "  border: 0;"
        "  padding: 0;"
        "  margin: 2px;"
        "  border-radius: 4px;"
        "}"
        "QToolButton#pushButton_closeDoc:hover,"
        "QToolButton#pushButton_splitView:hover {"
        "  background-color: %4;"
        "  color: %3;"
        "}"
        "QToolButton#pushButton_closeDoc:pressed,"
        "QToolButton#pushButton_splitView:pressed {"
        "  background-color: %5;"
        "  color: %3;"
        "}"
        "QToolButton#pushButton_splitView:checked {"
        "  background-color: %5;"
        "  color: %3;"
        "}");

    ui->topbar->setStyleSheet(topbarStyle.arg(background().name(QColor::HexRgb),
                                             border().name(QColor::HexRgb),
                                             text().name(QColor::HexRgb),
                                             hoverBg().name(QColor::HexRgb),
                                             pressedBg().name(QColor::HexRgb),
                                             mutedText().name(QColor::HexRgb)));

    if (QAbstractItemView *view = ui->comboBox_files->view())
    {
        view->setMouseTracking(true);
        view->viewport()->setMouseTracking(true);
        view->setStyleSheet(QStringLiteral(
                                "QAbstractItemView {"
                                "  background-color: %1;"
                                "  color: %2;"
                                "  border: 1px solid %3;"
                                "  outline: 0;"
                                "  selection-background-color: %4;"
                                "  selection-color: %5;"
                                "}"
                                "QAbstractItemView::item {"
                                "  min-height: 22px;"
                                "  padding: 2px 8px;"
                                "}"
                                "QAbstractItemView::item:hover {"
                                "  background-color: %6;"
                                "  color: %2;"
                                "}")
                                .arg(popupBg().name(QColor::HexRgb),
                                     text().name(QColor::HexRgb),
                                     border().name(QColor::HexRgb),
                                     selectionBg().name(QColor::HexRgb),
                                     selectionText().name(QColor::HexRgb),
                                     hoverBg().name(QColor::HexRgb)));
    }
}

void EditorPane::syncComboBoxIndex(int index)
{
    if (index >= 0 && index < ui->comboBox_files->count())
    {
        const QSignalBlocker blocker(ui->comboBox_files);
        ui->comboBox_files->setCurrentIndex(index);
        if (QAbstractItemView *view = ui->comboBox_files->view())
        {
            view->viewport()->update();
        }
    }
}
