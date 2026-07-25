#include "widgets/searchresultsdock.h"

#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace {

enum ItemDataRole
{
    FilePathRole = Qt::UserRole,
    LineRole,
    ColumnRole,
    LengthRole
};

} // namespace

SearchResultsDock::SearchResultsDock(QWidget *parent)
    : QObject(parent)
    , m_dock(new QDockWidget(tr("Search Results"), parent))
    , m_patternEdit(new QLineEdit)
    , m_caseButton(new QToolButton)
    , m_wholeWordButton(new QToolButton)
    , m_searchButton(new QPushButton(tr("Search")))
    , m_statusLabel(new QLabel)
    , m_resultsTree(new QTreeWidget)
    , m_searcher(new FileSearcher(this))
{
    m_patternEdit->setPlaceholderText(tr("Search in files..."));
    m_patternEdit->setClearButtonEnabled(true);

    m_caseButton->setText(QStringLiteral("Aa"));
    m_caseButton->setToolTip(tr("Match case"));
    m_caseButton->setCheckable(true);

    m_wholeWordButton->setText(QStringLiteral("W"));
    m_wholeWordButton->setToolTip(tr("Whole words only"));
    m_wholeWordButton->setCheckable(true);

    m_statusLabel->setVisible(false);

    m_resultsTree->setHeaderHidden(true);
    m_resultsTree->setUniformRowHeights(true);
    m_resultsTree->setTextElideMode(Qt::ElideRight);
    m_resultsTree->setExpandsOnDoubleClick(false);

    auto *queryRow = new QHBoxLayout;
    queryRow->setContentsMargins(0, 0, 0, 0);
    queryRow->setSpacing(4);
    queryRow->addWidget(m_patternEdit, 1);
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
    m_dock->setWidget(container);

    connect(m_patternEdit, &QLineEdit::returnPressed,
            this, &SearchResultsDock::startOrCancelSearch);
    connect(m_searchButton, &QPushButton::clicked,
            this, &SearchResultsDock::startOrCancelSearch);
    connect(m_resultsTree, &QTreeWidget::itemActivated,
            this, &SearchResultsDock::onItemActivated);
    connect(m_searcher, &FileSearcher::searchFinished,
            this, &SearchResultsDock::onSearchFinished);
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
                      m_wholeWordButton->isChecked());
}

void SearchResultsDock::onSearchFinished(const FileSearchOutcome &outcome)
{
    setSearchingState(false);

    QTreeWidgetItem *currentFileItem = nullptr;
    QString currentFilePath;
    for (const FileSearchMatch &match : outcome.matches)
    {
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
        matchItem->setText(0, tr("%1: %2")
                                  .arg(match.line + 1)
                                  .arg(match.lineText.trimmed()));
        matchItem->setToolTip(0, match.lineText);
        matchItem->setData(0, FilePathRole, match.filePath);
        matchItem->setData(0, LineRole, match.line);
        matchItem->setData(0, ColumnRole, match.column);
        matchItem->setData(0, LengthRole, match.length);
    }

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
    m_caseButton->setEnabled(!searching);
    m_wholeWordButton->setEnabled(!searching);
}
