#ifndef DOCUMENTIO_H
#define DOCUMENTIO_H

#include <QString>

#include "editor/documentformat.h"
#include <QByteArray>

class DocumentIO
{
public:
    static bool loadTextFile(const QString &filePath, QString *content, QString *errorMessage);
    static bool loadTextFile(const QString &filePath,
                             QString *content,
                             DocumentFormat *format,
                             QString *errorMessage);
    static bool loadEditorFile(const QString &filePath,
                               QByteArray *utf8Content,
                               DocumentFormat *format,
                               QString *errorMessage);
    static bool saveTextFile(const QString &filePath, const QString &content, QString *errorMessage);
    static bool saveTextFile(const QString &filePath,
                             const QString &content,
                             const DocumentFormat &format,
                             QString *errorMessage);
    static bool saveUtf8File(const QString &filePath, const QByteArray &content, QString *errorMessage);
};

#endif // DOCUMENTIO_H
