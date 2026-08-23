#ifndef GITEXECUTABLE_H
#define GITEXECUTABLE_H

#include "versioncontrol/gittypes.h"

#include <QByteArray>
#include <QString>

namespace GitExecutable
{

QString find(const QString &configuredPath = QString());
GitVersion parseVersion(const QByteArray &versionOutput);

} // namespace GitExecutable

#endif // GITEXECUTABLE_H
