#include "widgets/filetypefilterproxymodel.h"

#include "editor/performanceconstants.h"
#include "editor/documentpath.h"
#include "editor/supportedfilepolicy.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFutureWatcher>
#include <QFont>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent>

namespace {

QString normalizedAbsolutePath(const QString &path)
{
    return DocumentPath::normalized(path);
}

QString lexicalPathKey(const QString &path)
{
    QString key = QDir::cleanPath(QDir::fromNativeSeparators(
        QFileInfo(path).absoluteFilePath()));
#ifdef Q_OS_WIN
    key = key.toCaseFolded();
#endif
    return key;
}

bool isPathKeyInsideDirectory(const QString &pathKey, QString directoryKey)
{
    if (pathKey == directoryKey)
        return true;
    if (!directoryKey.endsWith(QLatin1Char('/')))
        directoryKey += QLatin1Char('/');
    return pathKey.startsWith(directoryKey);
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
    , m_supportPool(new QThreadPool(this))
    , m_filterRefreshTimer(new QTimer(this))
{
    setRecursiveFilteringEnabled(true);
    m_supportCache.setMaxCost(PerformanceConstants::SupportCacheLimit);
    m_supportPool->setMaxThreadCount(2);
    m_supportPool->setExpiryTimeout(10000);
    m_filterRefreshTimer->setSingleShot(true);
    m_filterRefreshTimer->setInterval(40);
    connect(m_filterRefreshTimer, &QTimer::timeout, this, [this]() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        invalidateFilter();
#endif
    });
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
    m_visibleRootPathKey = lexicalPathKey(dirPath);
    m_visibleRootCanonicalPathKey = lexicalPathKey(normalizedPath);
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
    m_visibleRootPathKey = lexicalPathKey(dirPath);
    m_visibleRootCanonicalPathKey = lexicalPathKey(normalizedPath);
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
    // Long names are elided in the tree, so the untruncated name has to stay
    // reachable somewhere.  QFileSystemModel supplies no tooltip of its own.
    if (index.isValid() && role == Qt::ToolTipRole)
    {
        return QSortFilterProxyModel::data(index, Qt::DisplayRole);
    }

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
        QString filePathKey = lexicalPathKey(filePath);
        if (model->fileInfo(index).isSymLink())
        {
            // Preserve the old containment rule for the comparatively rare
            // symlink entry without canonicalizing every ordinary tree row.
            filePathKey = lexicalPathKey(normalizedAbsolutePath(filePath));
        }
        const bool isRoot = filePathKey == m_visibleRootPathKey
                            || filePathKey == m_visibleRootCanonicalPathKey;
        if (isRoot)
        {
            return true;
        }

        const bool isRootAncestor =
            isPathKeyInsideDirectory(m_visibleRootPathKey, filePathKey)
            || isPathKeyInsideDirectory(m_visibleRootCanonicalPathKey, filePathKey);
        if (isRootAncestor)
        {
            return true;
        }

        const bool isInsideRoot =
            isPathKeyInsideDirectory(filePathKey, m_visibleRootPathKey)
            || isPathKeyInsideDirectory(filePathKey, m_visibleRootCanonicalPathKey);
        if (!isInsideRoot)
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

    const QString normalizedPath = lexicalPathKey(filePath);
    if (normalizedPath == m_visibleRootPathKey
        || normalizedPath == m_visibleRootCanonicalPathKey
        || isPathKeyInsideDirectory(m_visibleRootPathKey, normalizedPath)
        || isPathKeyInsideDirectory(m_visibleRootCanonicalPathKey, normalizedPath))
    {
        return false;
    }

    const QString name = QFileInfo(filePath).fileName();
    return m_ignoredDirectoryNames.contains(name)
           || name.startsWith(QStringLiteral("cmake-build-"));
}

bool FileTypeFilterProxyModel::isSupportedTextFile(const QString &filePath) const
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.isReadable() || !fileInfo.isFile())
    {
        return false;
    }

    const QString absolutePath = fileInfo.absoluteFilePath();
    if (!SupportedFilePolicy::hasSupportedTextType(fileInfo))
        return false;
    if (fileInfo.size() == 0)
        return true;

    if (const CacheEntry *cached = m_supportCache.object(absolutePath);
        cached && cached->size == fileInfo.size()
        && cached->lastModified == fileInfo.lastModified())
    {
        return cached->supported;
    }

    // QFileSystemModel discovers entries off-thread, but proxy filtering itself
    // runs on the GUI thread. Keep eligible files visible while the 4 KiB binary
    // probe runs in a bounded pool; a coalesced filter refresh removes a binary
    // entry once classification completes.
    scheduleSupportCheck(absolutePath, fileInfo.size(), fileInfo.lastModified());
    return true;
}

void FileTypeFilterProxyModel::scheduleSupportCheck(
    const QString &filePath, qint64 size, const QDateTime &lastModified) const
{
    if (m_supportChecksInFlight.contains(filePath))
        return;

    m_supportChecksInFlight.insert(filePath);
    auto *watcher = new QFutureWatcher<bool>(const_cast<FileTypeFilterProxyModel *>(this));
    connect(watcher, &QFutureWatcher<bool>::finished,
            const_cast<FileTypeFilterProxyModel *>(this),
            [this, watcher, filePath, size, lastModified]() {
        const bool supported = watcher->result();
        watcher->deleteLater();
        m_supportChecksInFlight.remove(filePath);

        const QFileInfo current(filePath);
        if (current.isFile() && current.size() == size
            && current.lastModified() == lastModified)
        {
            m_supportCache.insert(filePath,
                                  new CacheEntry{size, lastModified, supported});
        }
        // Refresh even when the file changed during the probe. In that case
        // the next filter pass schedules a classification for the new
        // metadata instead of leaving the provisional row visible forever.
        m_filterRefreshTimer->start();
    });
    watcher->setFuture(QtConcurrent::run(m_supportPool, [filePath]() {
        return SupportedFilePolicy::isSupportedTextFile(filePath);
    }));
}
