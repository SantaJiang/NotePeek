#ifndef SUPPORTEDFILEPOLICY_H
#define SUPPORTEDFILEPOLICY_H

#include <QByteArray>
#include <QString>

class QFileInfo;

class SupportedFilePolicy
{
public:
    static bool isSupportedTextFile(const QString &filePath);
    static bool hasSupportedTextType(const QFileInfo &fileInfo);
    static bool looksLikeBinaryFile(const QString &filePath);
    static bool looksLikeBinaryData(const QByteArray &bytes);
};

#endif // SUPPORTEDFILEPOLICY_H
