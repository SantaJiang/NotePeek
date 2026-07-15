#include "editor/documentio.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

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
    QFile file(filePath);
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif

    if (content)
    {
        *content = in.readAll();
    }
    return true;
}

bool DocumentIO::saveTextFile(const QString &filePath, const QString &content, QString *errorMessage)
{
    QSaveFile file(resolvedWritePath(filePath));
    if (!file.open(QFile::WriteOnly | QFile::Text))
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }

    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif

    out << content;
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit())
    {
        if (errorMessage)
        {
            *errorMessage = file.errorString();
        }
        return false;
    }
    return true;
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
