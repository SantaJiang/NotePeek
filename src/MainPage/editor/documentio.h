#ifndef DOCUMENTIO_H
#define DOCUMENTIO_H

#include <QString>

class DocumentIO
{
public:
    static bool loadTextFile(const QString &filePath, QString *content, QString *errorMessage);
    static bool saveTextFile(const QString &filePath, const QString &content, QString *errorMessage);
};

#endif // DOCUMENTIO_H
