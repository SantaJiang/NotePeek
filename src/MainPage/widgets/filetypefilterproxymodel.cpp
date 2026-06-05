#include "filetypefilterproxymodel.h"

#include "../editor/lexerfactory.h"

#include <QFile>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QSet>

namespace {

constexpr qint64 BinaryProbeSize = 4096;

} // namespace

FileTypeFilterProxyModel::FileTypeFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
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

    if (model->isDir(index))
    {
        return true;
    }

    return isSupportedTextFile(model->filePath(index));
}

QFileSystemModel *FileTypeFilterProxyModel::fileSystemModel() const
{
    return qobject_cast<QFileSystemModel *>(sourceModel());
}

bool FileTypeFilterProxyModel::isSupportedTextFile(const QString &filePath) const
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.isReadable() || !fileInfo.isFile())
    {
        return false;
    }

    const QString absolutePath = fileInfo.absoluteFilePath();
    const CacheEntry cached = m_supportCache.value(absolutePath);
    if (m_supportCache.contains(absolutePath)
        && cached.size == fileInfo.size()
        && cached.lastModified == fileInfo.lastModified())
    {
        return cached.supported;
    }

    const bool supported = hasSupportedTextType(fileInfo) && !looksLikeBinaryFile(absolutePath);
    m_supportCache.insert(absolutePath, { fileInfo.size(), fileInfo.lastModified(), supported });
    return supported;
}

bool FileTypeFilterProxyModel::hasSupportedTextType(const QFileInfo &fileInfo) const
{
    const QString filePath = fileInfo.absoluteFilePath();
    if (LexerFactory::languageFromFilePath(filePath) != LexerFactory::Unknown)
    {
        return true;
    }

    const QString suffix = fileInfo.suffix().toLower();
    static const QSet<QString> plainTextExtensions = {
        QStringLiteral("txt"),
        QStringLiteral("text"),
        QStringLiteral("log"),
        QStringLiteral("csv"),
        QStringLiteral("tsv")
    };
    if (plainTextExtensions.contains(suffix))
    {
        return true;
    }

    const QString fileName = fileInfo.fileName().toLower();
    static const QSet<QString> plainTextFileNames = {
        QStringLiteral("license"),
        QStringLiteral("copying"),
        QStringLiteral("notice"),
        QStringLiteral("readme"),
        QStringLiteral("changelog"),
        QStringLiteral("authors"),
        QStringLiteral("todo")
    };
    return plainTextFileNames.contains(fileName);
}

bool FileTypeFilterProxyModel::looksLikeBinaryFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
    {
        return true;
    }

    const QByteArray sample = file.read(BinaryProbeSize);
    if (sample.isEmpty())
    {
        return false;
    }

    int controlCharacters = 0;
    for (char ch : sample)
    {
        const uchar byte = static_cast<uchar>(ch);
        if (byte == 0)
        {
            return true;
        }
        if (byte < 0x20 && byte != '\n' && byte != '\r' && byte != '\t' && byte != '\f')
        {
            controlCharacters++;
        }
    }

    return controlCharacters > sample.size() / 10;
}
