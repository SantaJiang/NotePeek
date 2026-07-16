#include "widgets/filetypefilterproxymodel.h"

#include "editor/performanceconstants.h"
#include "editor/documentpath.h"
#include "editor/supportedfilepolicy.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFont>

namespace {

QString normalizedAbsolutePath(const QString &path)
{
    return DocumentPath::normalized(path);
}

bool isPathInsideDirectory(const QString &path, const QString &directory)
{
    return DocumentPath::isInsideDirectory(path, directory);
}

bool isAncestorOfPath(const QString &candidateAncestor, const QString &path)
{
    const QString normalizedAncestor = normalizedAbsolutePath(candidateAncestor);
    const QString normalizedPath = normalizedAbsolutePath(path);
    return normalizedAncestor != normalizedPath
           && isPathInsideDirectory(normalizedPath, normalizedAncestor);
}

} // namespace

FileTypeFilterProxyModel::FileTypeFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_ignoredDirectoryNames({
          QStringLiteral(".git"),
          QStringLiteral(".svn"),
          QStringLiteral(".hg"),
          QStringLiteral("build"),
          QStringLiteral("cmake-build-debug"),
          QStringLiteral("cmake-build-release"),
          QStringLiteral("node_modules"),
          QStringLiteral("DerivedData"),
          QStringLiteral("__pycache__")
      })
{
    setRecursiveFilteringEnabled(true);
}

void FileTypeFilterProxyModel::setVisibleRootDirectory(const QString &dirPath)
{
    const QString normalizedPath = normalizedAbsolutePath(dirPath);
    if (m_visibleRootDirectory == normalizedPath)
    {
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    m_visibleRootDirectory = normalizedPath;
    if (QFileSystemModel *model = fileSystemModel())
    {
        m_visibleRootIndex = model->index(m_visibleRootDirectory);
    }
    else
    {
        m_visibleRootIndex = QPersistentModelIndex();
    }
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    m_visibleRootDirectory = normalizedPath;
    if (QFileSystemModel *model = fileSystemModel())
    {
        m_visibleRootIndex = model->index(m_visibleRootDirectory);
    }
    else
    {
        m_visibleRootIndex = QPersistentModelIndex();
    }
    invalidateFilter();
#endif
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

QVariant FileTypeFilterProxyModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid()
        && role == Qt::FontRole
        && !m_visibleRootDirectory.isEmpty()
        && mapToSource(index) == m_visibleRootIndex)
    {
        QFont font = QSortFilterProxyModel::data(index, role).value<QFont>();
        font.setBold(true);
        return font;
    }

    return QSortFilterProxyModel::data(index, role);
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

    const QString filePath = model->filePath(index);
    if (!m_visibleRootDirectory.isEmpty())
    {
        if (normalizedAbsolutePath(filePath) == m_visibleRootDirectory)
        {
            return true;
        }

        if (isAncestorOfPath(filePath, m_visibleRootDirectory))
        {
            return true;
        }

        if (!isPathInsideDirectory(filePath, m_visibleRootDirectory))
        {
            return false;
        }
    }

    if (model->isDir(index))
    {
        return !isIgnoredDirectory(filePath);
    }

    return isSupportedTextFile(filePath);
}

QFileSystemModel *FileTypeFilterProxyModel::fileSystemModel() const
{
    return qobject_cast<QFileSystemModel *>(sourceModel());
}

bool FileTypeFilterProxyModel::isIgnoredDirectory(const QString &filePath) const
{
    if (m_visibleRootDirectory.isEmpty())
    {
        return false;
    }

    const QString normalizedPath = normalizedAbsolutePath(filePath);
    if (normalizedPath == m_visibleRootDirectory || isAncestorOfPath(filePath, m_visibleRootDirectory))
    {
        return false;
    }

    return m_ignoredDirectoryNames.contains(QFileInfo(filePath).fileName());
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
    if (m_supportCache.size() >= PerformanceConstants::SupportCacheLimit)
    {
        m_supportCache.clear();
    }
    m_supportCache.insert(absolutePath, { fileInfo.size(), fileInfo.lastModified(), supported });
    return supported;
}
