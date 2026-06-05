#ifndef FILETYPEFILTERPROXYMODEL_H
#define FILETYPEFILTERPROXYMODEL_H

#include <QDateTime>
#include <QHash>
#include <QSortFilterProxyModel>

class QFileInfo;
class QFileSystemModel;

class FileTypeFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit FileTypeFilterProxyModel(QObject *parent = nullptr);

    QString filePath(const QModelIndex &proxyIndex) const;
    bool isDir(const QModelIndex &proxyIndex) const;

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
    bool isSupportedTextFile(const QString &filePath) const;
    bool hasSupportedTextType(const QFileInfo &fileInfo) const;
    bool looksLikeBinaryFile(const QString &filePath) const;

    mutable QHash<QString, CacheEntry> m_supportCache;
};

#endif // FILETYPEFILTERPROXYMODEL_H
