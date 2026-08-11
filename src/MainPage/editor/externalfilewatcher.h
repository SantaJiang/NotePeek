#ifndef EXTERNALFILEWATCHER_H
#define EXTERNALFILEWATCHER_H

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QFileSystemWatcher;
class QTimer;

class ExternalFileWatcher : public QObject
{
    Q_OBJECT

public:
    explicit ExternalFileWatcher(QObject *parent = nullptr);

    void setTrackedFilePaths(QObject *owner, const QSet<QString> &filePaths);
    void recordFileState(const QString &filePath);
    void refreshWatchedFiles();
    void requestExternalChangeCheck(const QString &filePath);

signals:
    void externalFileChanged(const QString &filePath);

private slots:
    void watchedFileChanged(const QString &filePath);
    void watchedDirectoryChanged(const QString &directoryPath);

private:
    struct WatchedFileState
    {
        QDateTime lastModified;
        QByteArray contentHash;
        qint64 size = -1;
        bool exists = false;
    };

    QString normalizedFilePath(const QString &filePath) const;
    void rebuildTrackedFilePaths();
    void startBaselineHash(const QString &filePath, const WatchedFileState &state);
    void checkFileStateAsync(const QString &filePath);

    QFileSystemWatcher *m_fileWatcher;
    QTimer *m_changeCheckTimer;
    QSet<QString> m_trackedFilePaths;
    QHash<QObject *, QSet<QString>> m_pathsByOwner;
    QSet<QString> m_pendingChangeChecks;
    QHash<QString, WatchedFileState> m_fileStates;
    QSet<QString> m_baselineHashesInFlight;
    QHash<QString, WatchedFileState> m_pendingBaselineStates;
    QSet<QString> m_hashChecksInFlight;
    QSet<QString> m_hashRecheckRequested;
};

#endif // EXTERNALFILEWATCHER_H
