#include "foundation/documentpath.h"

#include <QDir>
#include <QFileInfo>

QString DocumentPath::normalized(const QString &path)
{
    if (path.isEmpty())
        return {};

    const QFileInfo info(path);
    QString resolved = info.canonicalFilePath();
    if (resolved.isEmpty())
    {
        const QString parent = QDir(info.absolutePath()).canonicalPath();
        resolved = parent.isEmpty() ? info.absoluteFilePath()
                                    : QDir(parent).filePath(info.fileName());
    }
    return QDir::cleanPath(QDir::fromNativeSeparators(resolved));
}

QString DocumentPath::identityKey(const QString &path)
{
    QString key = normalized(path);
#ifdef Q_OS_WIN
    key = key.toCaseFolded();
#endif
    return key;
}

bool DocumentPath::equivalent(const QString &left, const QString &right)
{
    const QString leftKey = identityKey(left);
    return !leftKey.isEmpty() && leftKey == identityKey(right);
}

bool DocumentPath::isInsideDirectory(const QString &path, const QString &directory)
{
    const QString normalizedPath = normalized(path);
    const QString normalizedDirectory = normalized(directory);
    if (normalizedPath.isEmpty() || normalizedDirectory.isEmpty())
        return false;

    const QString relative = QDir(normalizedDirectory).relativeFilePath(normalizedPath);
    return relative == QLatin1String(".")
           || (!relative.startsWith(QLatin1String("../"))
               && relative != QLatin1String("..")
               && !QDir::isAbsolutePath(relative));
}
