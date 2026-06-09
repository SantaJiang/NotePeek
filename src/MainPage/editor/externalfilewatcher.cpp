#include "externalfilewatcher.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

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
{
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &ExternalFileWatcher::watchedFileChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &ExternalFileWatcher::watchedDirectoryChanged);
}

void ExternalFileWatcher::setTrackedFilePaths(const QSet<QString> &filePaths)
{
    QSet<QString> normalizedPaths;
    for (const QString &filePath : filePaths)
    {
        const QString normalizedPath = normalizedFilePath(filePath);
        if (!normalizedPath.isEmpty())
        {
            normalizedPaths.insert(normalizedPath);
        }
    }

    m_trackedFilePaths = normalizedPaths;
    refreshWatchedFiles();
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
        state.contentHash = fileContentHash(normalizedPath);
    }
    m_fileStates.insert(normalizedPath, state);
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

    QSet<QString> directories;
    for (const QString &filePath : m_trackedFilePaths)
    {
        const QFileInfo fileInfo(filePath);
        if (fileInfo.exists() && !m_fileWatcher->files().contains(filePath))
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
    for (const QString &watchedDirectory : watchedDirectories)
    {
        if (!directories.contains(normalizedFilePath(watchedDirectory)))
        {
            m_fileWatcher->removePath(watchedDirectory);
        }
    }
    for (const QString &directoryPath : directories)
    {
        if (!m_fileWatcher->directories().contains(directoryPath))
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

    QTimer::singleShot(200, this, [this, normalizedPath]() {
        if (hasFileStateChanged(normalizedPath))
        {
            emit externalFileChanged(normalizedPath);
        }
        refreshWatchedFiles();
    });
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
    if (filePath.isEmpty())
    {
        return QString();
    }

    return QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
}

bool ExternalFileWatcher::hasFileStateChanged(const QString &filePath) const
{
    const QString normalizedPath = normalizedFilePath(filePath);
    if (normalizedPath.isEmpty() || !m_trackedFilePaths.contains(normalizedPath))
    {
        return false;
    }

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
        return true;
    }

    return newState.exists && oldState.contentHash != fileContentHash(normalizedPath);
}
