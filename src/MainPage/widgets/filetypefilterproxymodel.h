#ifndef FILETYPEFILTERPROXYMODEL_H
#define FILETYPEFILTERPROXYMODEL_H

#include <QDateTime>
#include <QHash>
#include <QPersistentModelIndex>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>

class QFileSystemModel;

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

    QString m_visibleRootDirectory;
    QPersistentModelIndex m_visibleRootIndex;
    QSet<QString> m_ignoredDirectoryNames;
    mutable QHash<QString, CacheEntry> m_supportCache;
};

#endif // FILETYPEFILTERPROXYMODEL_H
