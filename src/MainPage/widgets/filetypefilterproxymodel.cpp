#include "filetypefilterproxymodel.h"

#include "../editor/supportedfilepolicy.h"

#include <QFileInfo>
#include <QFileSystemModel>

FileTypeFilterProxyModel::FileTypeFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
}

QString FileTypeFilterProxyModel::filePath(const QModelIndex &proxyIndex) const
{
    QFileSystemModel *model = fileSystemModel();
    return model && proxyIndex.isValid() ? model->filePath(mapToSource(proxyIndex)) : QString();
}

bool FileTypeFilterProxyModel::isDir(const QModelIndex &proxyIndex) const
{
    QFileSystemModel *model = fileSystemModel();
    return model && proxyIndex.isValid() && model->isDir(mapToSource(proxyIndex));
}

bool FileTypeFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QFileSystemModel *model = fileSystemModel();
    if (!model)
    {
        return false;
    }

    const QModelIndex index = model->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
    {
        return false;
    }

    if (model->isDir(index))
    {
        return true;
    }

    return isSupportedTextFile(model->filePath(index));
}

QFileSystemModel *FileTypeFilterProxyModel::fileSystemModel() const
{
    return qobject_cast<QFileSystemModel *>(sourceModel());
}

bool FileTypeFilterProxyModel::isSupportedTextFile(const QString &filePath) const
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.isReadable() || !fileInfo.isFile())
    {
        return false;
    }

    const QString absolutePath = fileInfo.absoluteFilePath();
    const CacheEntry cached = m_supportCache.value(absolutePath);
    if (m_supportCache.contains(absolutePath)
        && cached.size == fileInfo.size()
        && cached.lastModified == fileInfo.lastModified())
    {
        return cached.supported;
    }

    const bool supported = SupportedFilePolicy::isSupportedTextFile(absolutePath);
    m_supportCache.insert(absolutePath, { fileInfo.size(), fileInfo.lastModified(), supported });
    return supported;
}
