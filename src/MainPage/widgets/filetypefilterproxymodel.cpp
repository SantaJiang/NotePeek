#include "widgets/filetypefilterproxymodel.h"

#include "editor/performanceconstants.h"
#include "editor/backgroundworkscheduler.h"
#include "editor/documentpath.h"
#include "editor/projectpathfilter.h"
#include "editor/supportedfilepolicy.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFutureWatcher>
#include <QFont>
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
    , m_filterRefreshTimer(new QTimer(this))
{
    setRecursiveFilteringEnabled(true);
    m_supportCache.setMaxCost(PerformanceConstants::SupportCacheLimit);
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

void FileTypeFilterProxyModel::setGitRepositorySnapshot(const GitRepositorySnapshot &snapshot)
{
    m_gitDecorations.clear();
    if (snapshot.repositoryRoot.isEmpty())
        return;

    const QString repositoryRootKey = lexicalPathKey(snapshot.repositoryRoot);
    for (const GitFileStatus &status : snapshot.files)
    {
        if (status.isIgnored())
            continue;

        const GitStatusKind kind = gitStatusKind(status);
        const QString absolutePath = QDir(snapshot.repositoryRoot).absoluteFilePath(status.path);
        const QString pathKey = lexicalPathKey(absolutePath);
        m_gitDecorations.insert(pathKey, GitDecoration{kind, gitStatusText(kind), 1});

        QString parentKey = lexicalPathKey(QFileInfo(absolutePath).absolutePath());
        while (isPathKeyInsideDirectory(parentKey, repositoryRootKey))
        {
            GitDecoration &directory = m_gitDecorations[parentKey];
            ++directory.changeCount;
            directory.text = QString::number(directory.changeCount);
            if (int(kind) > int(directory.kind))
                directory.kind = kind;
            if (parentKey == repositoryRootKey)
                break;
            const QString nextParent = lexicalPathKey(QFileInfo(parentKey).absolutePath());
            if (nextParent == parentKey)
                break;
            parentKey = nextParent;
        }
    }
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
    auto gitDecoration = m_gitDecorations.cend();
    const bool needsGitDecoration = role == GitStatusTextRole
                                    || role == GitStatusKindRole
                                    || role == Qt::ToolTipRole;
    if (needsGitDecoration && index.isValid() && !m_gitDecorations.isEmpty())
        gitDecoration = m_gitDecorations.constFind(lexicalPathKey(filePath(index)));
    if (gitDecoration != m_gitDecorations.cend())
    {
        if (role == GitStatusTextRole)
            return gitDecoration->text;
        if (role == GitStatusKindRole)
            return int(gitDecoration->kind);
    }

    // Long names are elided in the tree, so the untruncated name has to stay
    // reachable somewhere.  QFileSystemModel supplies no tooltip of its own.
    if (index.isValid() && role == Qt::ToolTipRole)
    {
        const QString name = QSortFilterProxyModel::data(index, Qt::DisplayRole).toString();
        if (gitDecoration == m_gitDecorations.cend())
            return name;
        if (isDir(index))
            return tr("%1\nGit changes: %2").arg(name).arg(gitDecoration->changeCount);
        return tr("%1\nGit status: %2").arg(name, gitDecoration->text);
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

FileTypeFilterProxyModel::GitStatusKind
FileTypeFilterProxyModel::gitStatusKind(const GitFileStatus &status)
{
    if (status.isConflict())
        return GitStatusKind::Conflict;
    if (status.isUntracked())
        return GitStatusKind::Untracked;
    if (status.recordType == GitChangeRecordType::RenamedOrCopied)
        return status.renameOrCopyScore.startsWith(QLatin1Char('C'))
                   ? GitStatusKind::Copied
                   : GitStatusKind::Renamed;
    if (status.indexStatus == QLatin1Char('D') || status.workTreeStatus == QLatin1Char('D'))
        return GitStatusKind::Deleted;
    if (status.indexStatus == QLatin1Char('A') || status.workTreeStatus == QLatin1Char('A'))
        return GitStatusKind::Added;
    return GitStatusKind::Modified;
}

QString FileTypeFilterProxyModel::gitStatusText(GitStatusKind kind)
{
    switch (kind)
    {
    case GitStatusKind::Modified:  return QStringLiteral("M");
    case GitStatusKind::Added:     return QStringLiteral("A");
    case GitStatusKind::Deleted:   return QStringLiteral("D");
    case GitStatusKind::Renamed:   return QStringLiteral("R");
    case GitStatusKind::Copied:    return QStringLiteral("C");
    case GitStatusKind::Untracked: return QStringLiteral("U");
    case GitStatusKind::Conflict:  return QStringLiteral("!");
    case GitStatusKind::None:      break;
    }
    return QString();
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
    return ProjectPathFilter::isStandardIgnoredDirectoryName(name);
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
    watcher->setFuture(QtConcurrent::run(BackgroundWorkScheduler::backgroundPool(), [filePath]() {
        return SupportedFilePolicy::isSupportedTextFile(filePath);
    }));
}
