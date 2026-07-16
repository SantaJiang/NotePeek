#include "editor/documentio.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace {

QString resolvedWritePath(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (info.isSymLink())
    {
        const QString target = info.canonicalFilePath();
        if (!target.isEmpty())
        {
            return target;
        }
    }
    return filePath;
}

} // namespace

bool DocumentIO::loadTextFile(const QString &filePath, QString *content, QString *errorMessage)
{
    return loadTextFile(filePath, content, nullptr, errorMessage);
}

bool DocumentIO::loadTextFile(const QString &filePath,
                              QString *content,
                              DocumentFormat *format,
                              QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly))
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }

    return DocumentFormatCodec::decode(file.readAll(), content, format, errorMessage);
}

bool DocumentIO::loadEditorFile(const QString &filePath,
                                QByteArray *utf8Content,
                                DocumentFormat *format,
                                QString *errorMessage)
{
    QString text;
    if (!loadTextFile(filePath, &text, format, errorMessage))
        return false;
    if (utf8Content)
        *utf8Content = text.toUtf8();
    return true;
}

bool DocumentIO::saveTextFile(const QString &filePath, const QString &content, QString *errorMessage)
{
    return saveTextFile(filePath, content, DocumentFormatCodec::defaultFormat(), errorMessage);
}

bool DocumentIO::saveTextFile(const QString &filePath,
                              const QString &content,
                              const DocumentFormat &format,
                              QString *errorMessage)
{
    return saveUtf8File(filePath, DocumentFormatCodec::encode(content, format), errorMessage);
}

bool DocumentIO::saveUtf8File(const QString &filePath, const QByteArray &content, QString *errorMessage)
{
    QSaveFile file(resolvedWritePath(filePath));
    if (!file.open(QFile::WriteOnly))
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }

    const char *data = content.constData();
    qint64 remaining = content.size();
    while (remaining > 0)
    {
        const qint64 written = file.write(data, remaining);
        if (written <= 0)
        {
            if (errorMessage)
            {
                *errorMessage = written < 0 ? file.errorString() : QStringLiteral("No data was written.");
            }
            return false;
        }
        data += written;
        remaining -= written;
    }

    if (!file.commit())
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }
    return true;
}
