#include "editor/supportedfilepolicy.h"

#include "editor/lexerfactory.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>

namespace {

constexpr qint64 BinaryProbeSize = 4096;

} // namespace

bool SupportedFilePolicy::isSupportedTextFile(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.isReadable() || !fileInfo.isFile())
    {
        return false;
    }

    return hasSupportedTextType(fileInfo) && !looksLikeBinaryFile(fileInfo.absoluteFilePath());
}

bool SupportedFilePolicy::hasSupportedTextType(const QFileInfo &fileInfo)
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

bool SupportedFilePolicy::looksLikeBinaryFile(const QString &filePath)
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
