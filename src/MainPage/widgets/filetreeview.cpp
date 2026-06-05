#include "filetreeview.h"

#include "filetypefilterproxymodel.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDrag>
#include <QFileSystemModel>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QUrl>

FileTreeView::FileTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setDragEnabled(true);
}

void FileTreeView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    if (!isFileIndex(index))
    {
        QTreeView::contextMenuEvent(event);
        return;
    }

    QMenu menu(this);
    QAction *openInNewWindow = menu.addAction(tr("Open in New Window"));
    QAction *selectedAction = menu.exec(event->globalPos());
    if (selectedAction == openInNewWindow)
    {
        emit openInNewWindowRequested(filePathForIndex(index));
    }
}

void FileTreeView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragStartPos = event->pos();
    }

    QTreeView::mousePressEvent(event);
}

void FileTreeView::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
    {
        QTreeView::mouseMoveEvent(event);
        return;
    }

    if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance())
    {
        QTreeView::mouseMoveEvent(event);
        return;
    }

    const QModelIndex index = indexAt(m_dragStartPos);
    if (!isFileIndex(index))
    {
        QTreeView::mouseMoveEvent(event);
        return;
    }

    const QString filePath = filePathForIndex(index);
    auto *mimeData = new QMimeData();
    mimeData->setUrls({ QUrl::fromLocalFile(filePath) });

    auto *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction);
    if (!window()->frameGeometry().contains(QCursor::pos()))
    {
        emit openInNewWindowRequested(filePath);
    }
    event->accept();
}

QString FileTreeView::filePathForIndex(const QModelIndex &index) const
{
    if (auto *filterModel = qobject_cast<FileTypeFilterProxyModel *>(model()))
    {
        return filterModel->filePath(index);
    }

    auto *fileModel = qobject_cast<QFileSystemModel *>(model());
    return fileModel && index.isValid() ? fileModel->filePath(index) : QString();
}

bool FileTreeView::isFileIndex(const QModelIndex &index) const
{
    if (auto *filterModel = qobject_cast<FileTypeFilterProxyModel *>(model()))
    {
        return index.isValid() && !filterModel->isDir(index);
    }

    auto *fileModel = qobject_cast<QFileSystemModel *>(model());
    return fileModel && index.isValid() && !fileModel->isDir(index);
}
