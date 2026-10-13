#include "widgets/fileexplorerdock.h"

#include "foundation/documentpath.h"
#include "themecolors.h"
#include "widgets/filetreeview.h"
#include "widgets/filetypefilterproxymodel.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QPainter>
#include <QSize>
#include <QStyle>
#include <QStyledItemDelegate>

namespace {

constexpr int kGitStatusLeftSpacing = 6;
constexpr int kGitStatusRightMargin = 6;
constexpr int kDirectoryGitStatusDiameter = 5;
const QString kDirectoryGitStatusMarker = QStringLiteral("●");

QString normalizedAbsolutePath(const QString &path)
{
    return DocumentPath::normalized(path);
}

QFont compactTreeFont()
{
    QFont font = qApp ? qApp->font() : QFont();
    if (font.pixelSize() > 0)
    {
        font.setPixelSize(qMin(font.pixelSize(), 14));
    }
    else
    {
        const qreal pointSize = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;
        font.setPointSizeF(qMin(pointSize, 10.5));
    }
    return font;
}

QColor gitStatusColor(FileTypeFilterProxyModel::GitStatusKind kind)
{
    using Kind = FileTypeFilterProxyModel::GitStatusKind;
    switch (kind)
    {
    case Kind::Added:
    case Kind::Copied:
    case Kind::Untracked:
        return ThemeColors::color(ThemeColors::ColorRole::LexerAdded);
    case Kind::Deleted:
        return ThemeColors::color(ThemeColors::ColorRole::LexerRemoved);
    case Kind::Conflict:
        return ThemeColors::color(ThemeColors::ColorRole::LexerError);
    case Kind::Renamed:
        return ThemeColors::color(ThemeColors::ColorRole::PrefAccent);
    case Kind::Modified:
        return ThemeColors::color(ThemeColors::ColorRole::LexerChanged);
    case Kind::None:
        break;
    }
    return ThemeColors::FileTree::text();
}

class GitStatusItemDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const QString status = index.data(FileTypeFilterProxyModel::GitStatusTextRole).toString();
        if (status.isEmpty() || index.column() != 0)
        {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        if (option.state.testFlag(QStyle::State_Selected))
            painter->fillRect(option.rect, ThemeColors::FileTree::selectionBg());
        else if (option.state.testFlag(QStyle::State_MouseOver))
            painter->fillRect(option.rect, ThemeColors::FileTree::hoverBg());

        const bool isDirectoryMarker = status == kDirectoryGitStatusMarker;
        const int markerWidth = isDirectoryMarker
                                    ? kDirectoryGitStatusDiameter
                                    : option.fontMetrics.horizontalAdvance(status);
        const int statusWidth = qMin(option.rect.width(),
                                     markerWidth
                                         + kGitStatusLeftSpacing
                                         + kGitStatusRightMargin);
        QStyleOptionViewItem contentOption(option);
        contentOption.rect.adjust(0, 0, -statusWidth, 0);
        QStyledItemDelegate::paint(painter, contentOption, index);

        const auto kind = static_cast<FileTypeFilterProxyModel::GitStatusKind>(
            index.data(FileTypeFilterProxyModel::GitStatusKindRole).toInt());
        painter->save();
        painter->setPen(gitStatusColor(kind));
        if (isDirectoryMarker)
        {
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(gitStatusColor(kind));
            painter->setPen(Qt::NoPen);
            const QRectF itemRect(option.rect);
            const QRectF markerRect(itemRect.right() - kGitStatusRightMargin
                                        - kDirectoryGitStatusDiameter,
                                    itemRect.center().y()
                                        - (kDirectoryGitStatusDiameter / 2.0),
                                    kDirectoryGitStatusDiameter,
                                    kDirectoryGitStatusDiameter);
            painter->drawEllipse(markerRect);
        }
        else
        {
            painter->drawText(option.rect.adjusted(0, 0, -kGitStatusRightMargin, 0),
                              Qt::AlignRight | Qt::AlignVCenter,
                              status);
        }
        painter->restore();
    }
};

} // namespace

FileExplorerDock::FileExplorerDock(QWidget *parent)
    : QObject(parent)
    , m_fileModel(new QFileSystemModel(this))
    , m_fileFilterProxy(new FileTypeFilterProxyModel(this))
    , m_treeView(new FileTreeView(parent))
    , m_dock(new QDockWidget(tr("Note Browser"), parent))
{
    m_fileModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);

    m_fileFilterProxy->setSourceModel(m_fileModel);
    m_fileFilterProxy->setDynamicSortFilter(true);

    m_treeView->setModel(m_fileFilterProxy);
    m_treeView->setItemDelegate(new GitStatusItemDelegate(m_treeView));
    // Keep expansion instant: QFileSystemModel populates directory children
    // asynchronously, so an animated expand of a not-yet-cached folder starts on
    // an empty node and jumps when rows arrive mid-animation (the "flicker").
    // Disabling animation makes folder expansion consistent and flicker-free.
    m_treeView->setAnimated(false);
    m_treeView->setAllColumnsShowFocus(true);
    m_treeView->setExpandsOnDoubleClick(true);
    m_treeView->setFrameShape(QFrame::NoFrame);
    m_treeView->setFont(compactTreeFont());
    m_treeView->setIconSize(QSize(18, 18));
    m_treeView->setIndentation(14);
    m_treeView->setItemsExpandable(true);
    m_treeView->setMouseTracking(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setSortingEnabled(true);
    m_treeView->sortByColumn(0, Qt::AscendingOrder);
    m_treeView->setTextElideMode(Qt::ElideRight);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // Elision is measured against the column width, not the viewport, so the
    // name column has to track the viewport for long names to end in "…"
    // instead of being clipped at the dock edge.  The header is hidden, so the
    // column can never be resized by hand and nothing is lost by stretching it.
    m_treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_treeView->setHeaderHidden(true);
    m_treeView->viewport()->setMouseTracking(true);
    m_treeView->hideColumn(1);
    m_treeView->hideColumn(2);
    m_treeView->hideColumn(3);

    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_dock->setWidget(m_treeView);
    m_dock->setTitleBarWidget(new QWidget(m_dock));

    reapplyTheme();

    connect(m_treeView, &QTreeView::doubleClicked, this, &FileExplorerDock::onTreeDoubleClicked);
    connect(m_treeView, &FileTreeView::openInNewWindowRequested, this, &FileExplorerDock::openInNewWindowRequested);
    connect(m_fileModel, &QFileSystemModel::directoryLoaded,
            this, [this](const QString &) { emit repositoryContentsChanged(); });
    connect(m_fileModel, &QAbstractItemModel::rowsInserted,
            this, [this](const QModelIndex &, int, int) { emit repositoryContentsChanged(); });
    connect(m_fileModel, &QAbstractItemModel::rowsRemoved,
            this, [this](const QModelIndex &, int, int) { emit repositoryContentsChanged(); });
    connect(m_fileModel, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex &, const QModelIndex &, const QList<int> &) {
                emit repositoryContentsChanged();
            });
}

QDockWidget *FileExplorerDock::dockWidget() const
{
    return m_dock;
}

void FileExplorerDock::setVisible(bool visible)
{
    m_dock->setVisible(visible);
}

void FileExplorerDock::reapplyTheme()
{
    using namespace ThemeColors::FileTree;

    // Keep the view's semantic palette roles owned by one styling mechanism.
    // Combining setPalette() with setStyleSheet() lets a repolish restore the
    // previous scheme's explicitly resolved text roles on some platforms.
    QString styleSheet = QStringLiteral(
                                  "QTreeView {"
                                  "  background-color: %1;"
                                  "  color: %2;"
                                  "  selection-background-color: %4;"
                                  "  selection-color: %5;"
                                  "  border: 0;"
                                  "  outline: 0;"
                                  "  show-decoration-selected: 1;"
                                  "}"
                                  "QTreeView::item {"
                                  "  min-height: 20px;"
                                  "  padding: 0 3px;"
                                  "  border: 0;"
                                  "}"
                                  "QTreeView::item:hover {"
                                  "  background-color: %3;"
                                  "  color: %2;"
                                  "}"
                                  "QTreeView::item:selected {"
                                  "  background-color: %4;"
                                  "  color: %5;"
                                  "}")
                                  .arg(background().name(QColor::HexRgb),
                                       text().name(QColor::HexRgb),
                                       hoverBg().name(QColor::HexRgb),
                                       selectionBg().name(QColor::HexRgb),
                                       selectionText().name(QColor::HexRgb));
#ifdef Q_OS_WIN
    styleSheet += QStringLiteral("QTreeView { border-top: 1px solid %1; }")
                      .arg(ThemeColors::EditorTopBar::border().name(QColor::HexRgb));
#endif
    if (m_treeView->styleSheet() != styleSheet)
    {
        m_treeView->setStyleSheet(styleSheet);
    }
}

void FileExplorerDock::setRootDirectory(const QString &dirPath)
{
    if (dirPath.isEmpty())
    {
        m_fileFilterProxy->setVisibleRootDirectory(QString());
        m_treeView->setRootIndex(QModelIndex());
        return;
    }

    const QString rootPath = normalizedAbsolutePath(dirPath);
    const QModelIndex sourceRootIndex = m_fileModel->setRootPath(rootPath);
    m_fileFilterProxy->setVisibleRootDirectory(rootPath);

    const QModelIndex sourceTreeRootIndex = sourceRootIndex.parent();
    const QModelIndex proxyTreeRootIndex = sourceTreeRootIndex.isValid()
                                               ? m_fileFilterProxy->mapFromSource(sourceTreeRootIndex)
                                               : QModelIndex();
    m_treeView->setRootIndex(proxyTreeRootIndex);

    const QModelIndex proxyRootIndex = m_fileFilterProxy->mapFromSource(sourceRootIndex);
    if (proxyRootIndex.isValid())
    {
        m_treeView->expand(proxyRootIndex);
    }
}

void FileExplorerDock::setGitRepositorySnapshot(const GitRepositorySnapshot &snapshot)
{
    m_fileFilterProxy->setGitRepositorySnapshot(snapshot);
    m_treeView->viewport()->update();
}

void FileExplorerDock::selectFile(const QString &filePath)
{
    const QModelIndex proxyIndex = proxyIndexForPath(filePath);
    if (!proxyIndex.isValid())
    {
        return;
    }

    m_treeView->scrollTo(proxyIndex);
    m_treeView->setCurrentIndex(proxyIndex);
}

void FileExplorerDock::clearSelection()
{
    m_treeView->clearSelection();
    m_treeView->setCurrentIndex(QModelIndex());
}

void FileExplorerDock::expandDirectory(const QString &dirPath)
{
    const QModelIndex proxyIndex = proxyIndexForPath(dirPath);
    if (proxyIndex.isValid())
    {
        m_treeView->expand(proxyIndex);
    }
}

void FileExplorerDock::onTreeDoubleClicked(const QModelIndex &index)
{
    if (!m_fileFilterProxy->isDir(index))
    {
        emit fileActivated(m_fileFilterProxy->filePath(index));
    }
}

QModelIndex FileExplorerDock::proxyIndexForPath(const QString &path) const
{
    if (path.isEmpty())
    {
        return QModelIndex();
    }

    const QModelIndex sourceIndex = m_fileModel->index(path);
    if (!sourceIndex.isValid())
    {
        return QModelIndex();
    }

    return m_fileFilterProxy->mapFromSource(sourceIndex);
}
