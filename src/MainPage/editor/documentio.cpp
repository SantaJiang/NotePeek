#include "editor/documentio.h"

#include <QFile>
#include <QTextStream>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

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
    QFile file(filePath);
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
    return true;
}
