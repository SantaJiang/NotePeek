#include "editor/externalfilewatcher.h"

#include "editor/performanceconstants.h"
#include "editor/documentpath.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QTimer>
#include <QThreadPool>
#include <QtConcurrent>

namespace {

QByteArray fileContentHash(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
    {
        return QByteArray();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
    {
        return QByteArray();
    }
    return hash.result();
}

} // namespace

ExternalFileWatcher::ExternalFileWatcher(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_changeCheckTimer(new QTimer(this))
    , m_hashPool(new QThreadPool(this))
{
    m_hashPool->setMaxThreadCount(1);
    m_changeCheckTimer->setSingleShot(true);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &ExternalFileWatcher::watchedFileChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &ExternalFileWatcher::watchedDirectoryChanged);
    connect(m_changeCheckTimer, &QTimer::timeout, this, [this]() {
        const QSet<QString> pending = m_pendingChangeChecks;
        m_pendingChangeChecks.clear();
        for (const QString &normalizedPath : pending)
            checkFileStateAsync(normalizedPath);
        refreshWatchedFiles();
    });
}

void ExternalFileWatcher::setTrackedFilePaths(QObject *owner, const QSet<QString> &filePaths)
{
    if (!owner)
        return;
    QSet<QString> normalizedPaths;
    for (const QString &filePath : filePaths)
    {
        const QString normalizedPath = normalizedFilePath(filePath);
        if (!normalizedPath.isEmpty())
        {
            normalizedPaths.insert(normalizedPath);
        }
    }

    const bool firstRegistration = !m_pathsByOwner.contains(owner);
    m_pathsByOwner.insert(owner, normalizedPaths);
    if (firstRegistration)
    {
        connect(owner, &QObject::destroyed, this, [this, owner]() {
            m_pathsByOwner.remove(owner);
            rebuildTrackedFilePaths();
            refreshWatchedFiles();
        });
    }
    rebuildTrackedFilePaths();
    refreshWatchedFiles();
}

void ExternalFileWatcher::rebuildTrackedFilePaths()
{
    m_trackedFilePaths.clear();
    for (auto it = m_pathsByOwner.cbegin(); it != m_pathsByOwner.cend(); ++it)
        m_trackedFilePaths.unite(it.value());
}

void ExternalFileWatcher::recordFileState(const QString &filePath)
{
    const QString normalizedPath = normalizedFilePath(filePath);
    if (normalizedPath.isEmpty())
    {
        return;
    }

    const QFileInfo fileInfo(normalizedPath);
    WatchedFileState state;
    state.exists = fileInfo.exists() && fileInfo.isFile();
    if (state.exists)
    {
        state.lastModified = fileInfo.lastModified();
        state.size = fileInfo.size();
    }
    m_fileStates.insert(normalizedPath, state);
    if (state.exists && state.size <= PerformanceConstants::LargeFileSize)
        startBaselineHash(normalizedPath, state);
}

void ExternalFileWatcher::refreshWatchedFiles()
{
    const QStringList watchedFiles = m_fileWatcher->files();
    for (const QString &watchedFile : watchedFiles)
    {
        if (!m_trackedFilePaths.contains(normalizedFilePath(watchedFile)))
        {
            m_fileWatcher->removePath(watchedFile);
        }
    }

    // Snapshot the watched sets once: QFileSystemWatcher::files()/directories()
    // rebuild a list on every call, which made these loops quadratic.
    QSet<QString> alreadyWatchedFiles;
    for (const QString &watchedFile : m_fileWatcher->files())
    {
        alreadyWatchedFiles.insert(watchedFile);
    }

    QSet<QString> directories;
    for (const QString &filePath : m_trackedFilePaths)
    {
        const QFileInfo fileInfo(filePath);
        if (fileInfo.exists() && !alreadyWatchedFiles.contains(filePath))
        {
            m_fileWatcher->addPath(filePath);
        }

        const QString directoryPath = QDir::cleanPath(fileInfo.absolutePath());
        if (!directoryPath.isEmpty())
        {
            directories.insert(directoryPath);
        }
    }

    const QStringList watchedDirectories = m_fileWatcher->directories();
    QSet<QString> alreadyWatchedDirectories;
    for (const QString &watchedDirectory : watchedDirectories)
    {
        alreadyWatchedDirectories.insert(watchedDirectory);
        if (!directories.contains(normalizedFilePath(watchedDirectory)))
        {
            m_fileWatcher->removePath(watchedDirectory);
        }
    }
    for (const QString &directoryPath : directories)
    {
        if (!alreadyWatchedDirectories.contains(directoryPath))
        {
            m_fileWatcher->addPath(directoryPath);
        }
    }

    for (auto it = m_fileStates.begin(); it != m_fileStates.end();)
    {
        if (!m_trackedFilePaths.contains(it.key()))
        {
            it = m_fileStates.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void ExternalFileWatcher::requestExternalChangeCheck(const QString &filePath)
{
    const QString normalizedPath = normalizedFilePath(filePath);
    if (normalizedPath.isEmpty() || !m_trackedFilePaths.contains(normalizedPath))
    {
        refreshWatchedFiles();
        return;
    }

    m_pendingChangeChecks.insert(normalizedPath);
    m_changeCheckTimer->start(PerformanceConstants::WatcherDirectoryDebounceMs);
}

void ExternalFileWatcher::watchedFileChanged(const QString &filePath)
{
    requestExternalChangeCheck(filePath);
}

void ExternalFileWatcher::watchedDirectoryChanged(const QString &directoryPath)
{
    const QString changedDirectory = QDir::cleanPath(QFileInfo(directoryPath).absoluteFilePath());
    for (const QString &filePath : m_trackedFilePaths)
    {
        if (QDir::cleanPath(QFileInfo(filePath).absolutePath()) == changedDirectory)
        {
            requestExternalChangeCheck(filePath);
        }
    }

    refreshWatchedFiles();
}

QString ExternalFileWatcher::normalizedFilePath(const QString &filePath) const
{
    return DocumentPath::normalized(filePath);
}

void ExternalFileWatcher::startBaselineHash(const QString &filePath,
                                            const WatchedFileState &expectedState)
{
    auto *watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this,
            [this, watcher, filePath, expectedState]() {
        const QByteArray hash = watcher->result();
        watcher->deleteLater();
        auto it = m_fileStates.find(filePath);
        if (it != m_fileStates.end()
            && it->exists == expectedState.exists
            && it->size == expectedState.size
            && it->lastModified == expectedState.lastModified)
        {
            it->contentHash = hash;
        }
    });
    watcher->setFuture(QtConcurrent::run(m_hashPool, [filePath]() { return fileContentHash(filePath); }));
}

void ExternalFileWatcher::checkFileStateAsync(const QString &filePath)
{
    const QString normalizedPath = normalizedFilePath(filePath);
    if (normalizedPath.isEmpty() || !m_trackedFilePaths.contains(normalizedPath))
        return;

    const QFileInfo fileInfo(normalizedPath);
    WatchedFileState newState;
    newState.exists = fileInfo.exists() && fileInfo.isFile();
    if (newState.exists)
    {
        newState.lastModified = fileInfo.lastModified();
        newState.size = fileInfo.size();
    }

    const WatchedFileState oldState = m_fileStates.value(normalizedPath);
    if (oldState.exists != newState.exists
        || oldState.lastModified != newState.lastModified
        || oldState.size != newState.size)
    {
        emit externalFileChanged(normalizedPath);
        return;
    }

    if (!newState.exists || oldState.contentHash.isEmpty() || newState.size > PerformanceConstants::LargeFileSize)
        return;
    if (m_hashChecksInFlight.contains(normalizedPath))
    {
        m_hashRecheckRequested.insert(normalizedPath);
        return;
    }

    m_hashChecksInFlight.insert(normalizedPath);
    auto *watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this,
            [this, watcher, normalizedPath, expectedHash = oldState.contentHash]() {
        const QByteArray currentHash = watcher->result();
        watcher->deleteLater();
        m_hashChecksInFlight.remove(normalizedPath);
        const bool recheckRequested = m_hashRecheckRequested.remove(normalizedPath);
        const WatchedFileState currentBaseline = m_fileStates.value(normalizedPath);
        if (!currentHash.isEmpty() && currentHash != expectedHash
            && currentBaseline.contentHash == expectedHash
            && m_trackedFilePaths.contains(normalizedPath))
        {
            emit externalFileChanged(normalizedPath);
        }
        else if (recheckRequested && m_trackedFilePaths.contains(normalizedPath))
        {
            checkFileStateAsync(normalizedPath);
        }
    });
    watcher->setFuture(QtConcurrent::run(m_hashPool, [normalizedPath]() {
        return fileContentHash(normalizedPath);
    }));
}
