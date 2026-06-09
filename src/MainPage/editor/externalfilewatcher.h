#ifndef EXTERNALFILEWATCHER_H
#define EXTERNALFILEWATCHER_H

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QFileSystemWatcher;

class ExternalFileWatcher : public QObject
{
    Q_OBJECT

public:
    explicit ExternalFileWatcher(QObject *parent = nullptr);

    void setTrackedFilePaths(const QSet<QString> &filePaths);
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
    bool hasFileStateChanged(const QString &filePath) const;

    QFileSystemWatcher *m_fileWatcher;
    QSet<QString> m_trackedFilePaths;
    QHash<QString, WatchedFileState> m_fileStates;
};

#endif // EXTERNALFILEWATCHER_H
