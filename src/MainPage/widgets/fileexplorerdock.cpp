#include "widgets/fileexplorerdock.h"

#include "editor/documentpath.h"
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
#include <QPalette>
#include <QSize>

namespace {

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

    QPalette palette = m_treeView->palette();
    palette.setColor(QPalette::Base, background());
    palette.setColor(QPalette::Window, background());
    palette.setColor(QPalette::Text, text());
    palette.setColor(QPalette::Highlight, selectionBg());
    palette.setColor(QPalette::HighlightedText, selectionText());
    if (m_treeView->palette() != palette)
    {
        m_treeView->setPalette(palette);
    }

    const QString styleSheet = QStringLiteral(
                                  "QTreeView {"
                                  "  background-color: %1;"
                                  "  color: %2;"
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
