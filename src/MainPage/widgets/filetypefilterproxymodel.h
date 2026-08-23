#ifndef FILETYPEFILTERPROXYMODEL_H
#define FILETYPEFILTERPROXYMODEL_H

#include <QDateTime>
#include <QCache>
#include <QHash>
#include <QPersistentModelIndex>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>

#include "versioncontrol/gittypes.h"

class QFileSystemModel;
class QTimer;

class FileTypeFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum DataRole
    {
        GitStatusTextRole = Qt::UserRole + 100,
        GitStatusKindRole
    };

    enum class GitStatusKind
    {
        None,
        Modified,
        Added,
        Deleted,
        Renamed,
        Copied,
        Untracked,
        Conflict
    };

    explicit FileTypeFilterProxyModel(QObject *parent = nullptr);

    void setVisibleRootDirectory(const QString &dirPath);
    void setGitRepositorySnapshot(const GitRepositorySnapshot &snapshot);
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

    struct GitDecoration
    {
        GitStatusKind kind = GitStatusKind::None;
        QString text;
        int changeCount = 0;
    };

    QFileSystemModel *fileSystemModel() const;
    bool isIgnoredDirectory(const QString &filePath) const;
    bool isSupportedTextFile(const QString &filePath) const;
    void scheduleSupportCheck(const QString &filePath,
                              qint64 size,
                              const QDateTime &lastModified) const;
    static GitStatusKind gitStatusKind(const GitFileStatus &status);
    static QString gitStatusText(GitStatusKind kind);

    QString m_visibleRootDirectory;
    QString m_visibleRootPathKey;
    QString m_visibleRootCanonicalPathKey;
    QPersistentModelIndex m_visibleRootIndex;
    mutable QCache<QString, CacheEntry> m_supportCache;
    mutable QSet<QString> m_supportChecksInFlight;
    QTimer *m_filterRefreshTimer;
    QHash<QString, GitDecoration> m_gitDecorations;
};

#endif // FILETYPEFILTERPROXYMODEL_H
