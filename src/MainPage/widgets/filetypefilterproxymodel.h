#ifndef FILETYPEFILTERPROXYMODEL_H
#define FILETYPEFILTERPROXYMODEL_H

#include <QDateTime>
#include <QCache>
#include <QPersistentModelIndex>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>

class QFileSystemModel;
class QThreadPool;
class QTimer;

class FileTypeFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit FileTypeFilterProxyModel(QObject *parent = nullptr);

    void setVisibleRootDirectory(const QString &dirPath);
    QString filePath(const QModelIndex &proxyIndex) const;
    bool isDir(const QModelIndex &proxyIndex) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    struct CacheEntry
    {
        qint64 size;
        QDateTime lastModified;
        bool supported;
    };

    QFileSystemModel *fileSystemModel() const;
    bool isIgnoredDirectory(const QString &filePath) const;
    bool isSupportedTextFile(const QString &filePath) const;
    void scheduleSupportCheck(const QString &filePath,
                              qint64 size,
                              const QDateTime &lastModified) const;

    QString m_visibleRootDirectory;
    QString m_visibleRootPathKey;
    QString m_visibleRootCanonicalPathKey;
    QPersistentModelIndex m_visibleRootIndex;
    QSet<QString> m_ignoredDirectoryNames;
    mutable QCache<QString, CacheEntry> m_supportCache;
    mutable QSet<QString> m_supportChecksInFlight;
    QThreadPool *m_supportPool;
    QTimer *m_filterRefreshTimer;
};

#endif // FILETYPEFILTERPROXYMODEL_H
