#include "fileexplorerdock.h"

#include "filetreeview.h"
#include "filetypefilterproxymodel.h"

#include <QDir>
#include <QDockWidget>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QScroller>

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
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(15);
    m_treeView->setSortingEnabled(true);
    m_treeView->sortByColumn(0, Qt::AscendingOrder);
    m_treeView->setColumnWidth(0, 250);
    m_treeView->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_treeView->setHeaderHidden(true);
    m_treeView->hideColumn(1);
    m_treeView->hideColumn(2);
    m_treeView->hideColumn(3);
    QScroller::grabGesture(m_treeView, QScroller::TouchGesture);

    m_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_dock->setWidget(m_treeView);
    m_dock->setTitleBarWidget(new QWidget(m_dock));

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

void FileExplorerDock::setRootDirectory(const QString &dirPath)
{
    if (dirPath.isEmpty())
    {
        m_treeView->setRootIndex(QModelIndex());
        return;
    }

    const QModelIndex rootIndex = m_fileModel->setRootPath(dirPath);
    const QModelIndex proxyRootIndex = m_fileFilterProxy->mapFromSource(rootIndex);
    m_treeView->setRootIndex(proxyRootIndex);
    m_treeView->expand(proxyRootIndex);
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
