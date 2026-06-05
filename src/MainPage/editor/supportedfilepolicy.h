#ifndef SUPPORTEDFILEPOLICY_H
#define SUPPORTEDFILEPOLICY_H

#include <QString>

class QFileInfo;

class SupportedFilePolicy
{
public:
    static bool isSupportedTextFile(const QString &filePath);
    static bool hasSupportedTextType(const QFileInfo &fileInfo);
    static bool looksLikeBinaryFile(const QString &filePath);
};

#endif // SUPPORTEDFILEPOLICY_H
