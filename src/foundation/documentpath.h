#ifndef NOTEPEEK_FOUNDATION_DOCUMENTPATH_H
#define NOTEPEEK_FOUNDATION_DOCUMENTPATH_H

#include <QString>

namespace DocumentPath {

QString normalized(const QString &path);
QString identityKey(const QString &path);
bool equivalent(const QString &left, const QString &right);
bool isInsideDirectory(const QString &path, const QString &directory);

} // namespace DocumentPath

#endif // NOTEPEEK_FOUNDATION_DOCUMENTPATH_H
