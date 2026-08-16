#include "widgets/searchresultsdock.h"
#include "widgets/iconfont.h"

#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextCharFormat>
#include <QTextLayout>
#include <QTextOption>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

#include "themecolors.h"

namespace {

enum ItemDataRole
{
    FilePathRole = Qt::UserRole,
    LineRole,
    ColumnRole,
    LengthRole,
    PreviewColumnRole
};

class SearchResultDelegate final : public QStyledItemDelegate
{
public:
    explicit SearchResultDelegate(QTreeWidget *tree)
        : QStyledItemDelegate(tree)
        , m_tree(tree)
    {
    }

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const QVariant previewColumnData = index.data(PreviewColumnRole);
        if (!previewColumnData.isValid())
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        QStyleOptionViewItem itemOption(option);
        initStyleOption(&itemOption, index);
        const QString lineText = itemOption.text;
        itemOption.text.clear();
        itemOption.icon = QIcon();
        QStyle *style = itemOption.widget ? itemOption.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &itemOption, painter, itemOption.widget);

        constexpr int HorizontalPadding = 6;
        const int lineNumberWidth = qMax(30,
                                         m_tree->property("searchResultLineNumberWidth").toInt());
        const QRect lineNumberRect(itemOption.rect.left() + HorizontalPadding,
                                   itemOption.rect.top(),
                                   lineNumberWidth - HorizontalPadding * 2,
                                   itemOption.rect.height());
        const int separatorX = itemOption.rect.left() + lineNumberWidth;
        const QRect textRect(separatorX + HorizontalPadding,
                             itemOption.rect.top(),
                             qMax(0, itemOption.rect.right() - separatorX - HorizontalPadding),
                             itemOption.rect.height());

        const bool selected = itemOption.state.testFlag(QStyle::State_Selected);
        const QColor textColor = selected
                                     ? itemOption.palette.color(QPalette::HighlightedText)
                                     : ThemeColors::Editor::foreground();
        const QColor lineNumberColor = selected
                                           ? textColor
                                           : ThemeColors::Editor::marginFg();
        const QColor separatorColor = selected
                                          ? textColor
                                          : ThemeColors::Editor::indentGuide();

        painter->save();
        painter->setClipRect(itemOption.rect);
        painter->setFont(itemOption.font);
        painter->setPen(lineNumberColor);
        painter->drawText(lineNumberRect,
                          Qt::AlignRight | Qt::AlignVCenter,
                          QString::number(index.data(LineRole).toInt() + 1));
        painter->setPen(separatorColor);
        painter->drawLine(separatorX, itemOption.rect.top() + 3,
                          separatorX, itemOption.rect.bottom() - 3);

        QTextLayout textLayout(lineText, itemOption.font);
        QTextOption textOption;
        textOption.setWrapMode(QTextOption::NoWrap);
        const QFontMetrics metrics(itemOption.font);
        textOption.setTabStopDistance(metrics.horizontalAdvance(QLatin1Char(' ')) * 4);
        textLayout.setTextOption(textOption);

        QTextCharFormat baseFormat;
        baseFormat.setForeground(textColor);
        QTextLayout::FormatRange baseRange;
        baseRange.start = 0;
        baseRange.length = lineText.size();
        baseRange.format = baseFormat;

        const int previewColumn = previewColumnData.toInt();
        const int highlightLength = qMin(index.data(LengthRole).toInt(),
                                         qMax(0, int(lineText.size()) - previewColumn));
        QVector<QTextLayout::FormatRange> formats = { baseRange };
        if (previewColumn >= 0 && highlightLength > 0)
        {
            QTextCharFormat matchFormat;
            matchFormat.setBackground(ThemeColors::FindReplace::highlightIndicator());
            matchFormat.setForeground(Qt::black);
            QTextLayout::FormatRange matchRange;
            matchRange.start = previewColumn;
            matchRange.length = highlightLength;
            matchRange.format = matchFormat;
            formats.append(matchRange);
        }
        textLayout.setFormats(formats);
        textLayout.beginLayout();
        QTextLine textLine = textLayout.createLine();
        if (textLine.isValid())
        {
            textLine.setLineWidth(textRect.width());
        }
        textLayout.endLayout();
        if (textLine.isValid())
        {
            const qreal y = textRect.top() + (textRect.height() - textLine.height()) / 2.0;
            textLayout.draw(painter, QPointF(textRect.left(), y));
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(size.height(), 24));
        return size;
    }

private:
    QTreeWidget *m_tree;
};

} // namespace

SearchResultsDock::SearchResultsDock(QWidget *parent)
    : QObject(parent)
    , m_dock(new QDockWidget(tr("Search Results"), parent))
    , m_patternEdit(new QLineEdit)
    , m_excludeEdit(new QLineEdit)
    , m_caseButton(new QToolButton)
    , m_wholeWordButton(new QToolButton)
    , m_searchButton(new QToolButton)
    , m_titleBar(new QWidget)
    , m_statusLabel(new QLabel)
    , m_resultsTree(new QTreeWidget)
    , m_searcher(new FileSearcher(this))
{
    m_searchButton->setText(tr("Search"));
    m_searchButton->setObjectName(QStringLiteral("searchResultsSearchButton"));
    m_searchButton->setToolButtonStyle(Qt::ToolButtonTextOnly);

    m_patternEdit->setPlaceholderText(tr("Search in files..."));
    m_patternEdit->setObjectName(QStringLiteral("searchResultsPatternEdit"));
    m_patternEdit->setClearButtonEnabled(true);

    m_excludeEdit->setPlaceholderText(tr("Exclude globs (comma-separated)"));
    m_excludeEdit->setObjectName(QStringLiteral("searchResultsExcludeEdit"));
    m_excludeEdit->setClearButtonEnabled(true);
    m_excludeEdit->setMaximumWidth(260);
    m_excludeEdit->setAccessibleName(tr("File exclusion patterns"));

    m_caseButton->setText(QStringLiteral("Aa"));
    m_caseButton->setObjectName(QStringLiteral("searchResultsCaseButton"));
    m_caseButton->setToolTip(tr("Match case"));
    m_caseButton->setCheckable(true);

    m_wholeWordButton->setText(QStringLiteral("W"));
    m_wholeWordButton->setObjectName(QStringLiteral("searchResultsWholeWordButton"));
    m_wholeWordButton->setToolTip(tr("Whole words only"));
    m_wholeWordButton->setCheckable(true);

    const int queryControlHeight = m_patternEdit->sizeHint().height();
    m_caseButton->setFixedHeight(queryControlHeight);
    m_wholeWordButton->setFixedHeight(queryControlHeight);
    m_searchButton->setFixedHeight(queryControlHeight);

    m_statusLabel->setVisible(false);

    m_resultsTree->setHeaderHidden(true);
    m_resultsTree->setIndentation(12);
    m_resultsTree->setUniformRowHeights(true);
    m_resultsTree->setTextElideMode(Qt::ElideRight);
    m_resultsTree->setExpandsOnDoubleClick(false);
    m_resultsTree->setProperty("searchResultLineNumberWidth", 30);
    auto *resultDelegate = new SearchResultDelegate(m_resultsTree);
    resultDelegate->setObjectName(QStringLiteral("searchResultDelegate"));
    m_resultsTree->setItemDelegate(resultDelegate);

    auto *queryRow = new QHBoxLayout;
    queryRow->setContentsMargins(0, 0, 0, 0);
    queryRow->setSpacing(4);
    queryRow->addWidget(m_patternEdit, 1);
    queryRow->addWidget(m_excludeEdit);
    queryRow->addWidget(m_caseButton);
    queryRow->addWidget(m_wholeWordButton);
    queryRow->addWidget(m_searchButton);

    auto *container = new QWidget;
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);
    layout->addLayout(queryRow);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_resultsTree, 1);

    m_dock->setObjectName(QStringLiteral("searchResultsDock"));
    m_dock->setFeatures(m_dock->features() & ~QDockWidget::DockWidgetFloatable);

    auto *titleLabel = new QLabel(tr("Search Results"), m_titleBar);
    titleLabel->setObjectName(QStringLiteral("searchResultsTitleLabel"));
    auto *closeButton = new QToolButton(m_titleBar);
    closeButton->setObjectName(QStringLiteral("searchResultsCloseButton"));
    closeButton->setFixedSize(26, 26);
    closeButton->setToolTip(tr("Close search results"));
    closeButton->setAccessibleName(tr("Close search results"));
    const QString &iconFontFamily = IconFont::fontAwesomeFamily();
    if (!iconFontFamily.isEmpty())
    {
        closeButton->setFont(QFont(iconFontFamily));
    }
    closeButton->setText(QChar(0xf00d));

    auto *titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(6, 0, 6, 0);
    titleLayout->setSpacing(4);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch(1);
    titleLayout->addWidget(closeButton);
    m_titleBar->setObjectName(QStringLiteral("searchResultsTitleBar"));
    m_titleBar->setAttribute(Qt::WA_StyledBackground, true);
    m_dock->setTitleBarWidget(m_titleBar);
    m_dock->setWidget(container);
    reapplyTheme();

    connect(m_patternEdit, &QLineEdit::returnPressed,
            this, &SearchResultsDock::startOrCancelSearch);
    connect(m_excludeEdit, &QLineEdit::returnPressed,
            this, &SearchResultsDock::startOrCancelSearch);
    connect(m_searchButton, &QToolButton::clicked,
            this, &SearchResultsDock::startOrCancelSearch);
    connect(closeButton, &QToolButton::clicked, m_dock, &QDockWidget::close);
    connect(m_resultsTree, &QTreeWidget::itemActivated,
            this, &SearchResultsDock::onItemActivated);
    connect(m_searcher, &FileSearcher::searchFinished,
            this, &SearchResultsDock::onSearchFinished);
}

void SearchResultsDock::reapplyTheme()
{
    using namespace ThemeColors::EditorTopBar;

    const QString titleStyle = QStringLiteral(
        "QWidget#searchResultsTitleBar {"
        "  background-color: %1;"
        "  border-top: 1px solid %2;"
        "}"
        "QLabel#searchResultsTitleLabel { color: %3; }"
        "QToolButton#searchResultsCloseButton {"
        "  background-color: transparent;"
        "  color: %3;"
        "  border: 0;"
        "  padding: 0;"
        "  margin: 0;"
        "  border-radius: 4px;"
        "}"
        "QToolButton#searchResultsCloseButton:hover { background-color: %4; }"
        "QToolButton#searchResultsCloseButton:pressed { background-color: %5; }"
    ).arg(background().name(QColor::HexRgb),
          border().name(QColor::HexRgb),
          ThemeColors::Editor::foreground().name(QColor::HexRgb),
          hoverBg().name(QColor::HexRgb),
          pressedBg().name(QColor::HexRgb));
    if (m_titleBar->styleSheet() != titleStyle)
    {
        m_titleBar->setStyleSheet(titleStyle);
    }

    const QString searchButtonStyle = QStringLiteral(
        "QToolButton {"
        "  background-color: transparent;"
        "  color: %1;"
        "  border: 0;"
        "  padding: 0 8px;"
        "  margin: 0;"
        "  border-radius: 4px;"
        "}"
        "QToolButton:hover { background-color: %2; }"
        "QToolButton:pressed { background-color: %3; }"
    ).arg(ThemeColors::Editor::foreground().name(QColor::HexRgb),
          hoverBg().name(QColor::HexRgb),
          pressedBg().name(QColor::HexRgb));
    if (m_searchButton->styleSheet() != searchButtonStyle)
    {
        m_searchButton->setStyleSheet(searchButtonStyle);
    }

    const QString fileTreeBg = ThemeColors::FileTree::background().name(QColor::HexRgb);
    const QString inputStyle = QStringLiteral(
        "QLineEdit { background-color: %1; }"
    ).arg(fileTreeBg);
    if (m_dock->widget()->styleSheet() != inputStyle)
    {
        m_dock->widget()->setStyleSheet(inputStyle);
    }

    const QString treeStyle = QStringLiteral(
        "QTreeWidget { background-color: %1; }"
    ).arg(fileTreeBg);
    if (m_resultsTree->styleSheet() != treeStyle)
    {
        m_resultsTree->setStyleSheet(treeStyle);
    }
}

void SearchResultsDock::setVisible(bool visible)
{
    m_dock->setVisible(visible);
}

void SearchResultsDock::setSearchRoot(const QString &rootDirectory)
{
    m_searchRoot = rootDirectory;
}

void SearchResultsDock::focusSearchInput(const QString &initialText)
{
    if (!initialText.isEmpty())
    {
        m_patternEdit->setText(initialText);
        m_patternEdit->selectAll();
    }
    m_patternEdit->setFocus();
}

void SearchResultsDock::startOrCancelSearch()
{
    if (m_searcher->isRunning())
    {
        m_searcher->cancel();
        setSearchingState(false);
        m_statusLabel->setText(tr("Search canceled."));
        m_statusLabel->setVisible(true);
        return;
    }

    const QString pattern = m_patternEdit->text();
    if (pattern.isEmpty())
    {
        return;
    }
    if (m_searchRoot.isEmpty())
    {
        m_statusLabel->setText(tr("Open a directory first to search in files."));
        m_statusLabel->setVisible(true);
        return;
    }

    m_resultsTree->clear();
    m_statusLabel->setText(tr("Searching in %1...").arg(QDir::toNativeSeparators(m_searchRoot)));
    m_statusLabel->setVisible(true);
    setSearchingState(true);
    m_searcher->start(m_searchRoot,
                      pattern,
                      m_caseButton->isChecked(),
                      m_wholeWordButton->isChecked(),
                      m_excludeEdit->text().split(
                          QRegularExpression(QStringLiteral("[,;]")), Qt::SkipEmptyParts));
}

void SearchResultsDock::onSearchFinished(const FileSearchOutcome &outcome)
{
    setSearchingState(false);
    const QSignalBlocker treeSignals(m_resultsTree);
    m_resultsTree->setUpdatesEnabled(false);

    QTreeWidgetItem *currentFileItem = nullptr;
    QString currentFilePath;
    int maximumLineNumber = 1;
    for (const FileSearchMatch &match : outcome.matches)
    {
        maximumLineNumber = qMax(maximumLineNumber, match.line + 1);
        if (!currentFileItem || currentFilePath != match.filePath)
        {
            currentFilePath = match.filePath;
            currentFileItem = new QTreeWidgetItem(m_resultsTree);
            currentFileItem->setText(0, QFileInfo(match.filePath).fileName());
            currentFileItem->setToolTip(0, QDir::toNativeSeparators(match.filePath));
            currentFileItem->setData(0, FilePathRole, match.filePath);
            currentFileItem->setFirstColumnSpanned(true);
        }

        auto *matchItem = new QTreeWidgetItem(currentFileItem);
        matchItem->setText(0, match.lineText);
        matchItem->setData(0, Qt::AccessibleTextRole,
                           tr("Line %1: %2").arg(match.line + 1).arg(match.lineText));
        matchItem->setToolTip(0, match.lineText);
        matchItem->setData(0, FilePathRole, match.filePath);
        matchItem->setData(0, LineRole, match.line);
        matchItem->setData(0, ColumnRole, match.column);
        matchItem->setData(0, LengthRole, match.length);
        matchItem->setData(0, PreviewColumnRole, match.previewColumn);
    }

    const QFontMetrics resultMetrics(m_resultsTree->font());
    const int lineNumberWidth = qMax(30,
                                     resultMetrics.horizontalAdvance(QString::number(maximumLineNumber)) + 12);
    m_resultsTree->setProperty("searchResultLineNumberWidth", lineNumberWidth);

    for (int index = 0; index < m_resultsTree->topLevelItemCount(); ++index)
    {
        QTreeWidgetItem *fileItem = m_resultsTree->topLevelItem(index);
        fileItem->setText(0, tr("%1 (%2)")
                                 .arg(fileItem->text(0))
                                 .arg(fileItem->childCount()));
        fileItem->setExpanded(true);
    }

    if (outcome.summary.canceled)
    {
        m_statusLabel->setText(tr("Search canceled."));
    }
    else if (outcome.summary.truncated)
    {
        m_statusLabel->setText(tr("Showing the first %1 matches; refine the search to see the rest.")
                                   .arg(FileSearcher::MaximumMatches));
    }
    else
    {
        m_statusLabel->setText(tr("%1 match(es) in %2 file(s).")
                                   .arg(outcome.summary.matchCount)
                                   .arg(m_resultsTree->topLevelItemCount()));
    }
    m_statusLabel->setVisible(true);
    m_resultsTree->setUpdatesEnabled(true);
    m_resultsTree->viewport()->update();
}

void SearchResultsDock::onItemActivated(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item)
    {
        return;
    }

    if (!item->parent())
    {
        item->setExpanded(!item->isExpanded());
        return;
    }

    emit matchActivated(item->data(0, FilePathRole).toString(),
                        item->data(0, LineRole).toInt(),
                        item->data(0, ColumnRole).toInt(),
                        item->data(0, LengthRole).toInt());
}

void SearchResultsDock::setSearchingState(bool searching)
{
    m_searchButton->setText(searching ? tr("Cancel") : tr("Search"));
    m_patternEdit->setEnabled(!searching);
    m_excludeEdit->setEnabled(!searching);
    m_caseButton->setEnabled(!searching);
    m_wholeWordButton->setEnabled(!searching);
}
