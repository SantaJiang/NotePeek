#ifndef GITSTATUSPARSER_H
#define GITSTATUSPARSER_H

#include "versioncontrol/gittypes.h"

#include <QByteArray>
#include <QString>

struct GitStatusParseResult
{
    GitRepositorySnapshot snapshot;
    QString error;

    bool succeeded() const
    {
        return error.isEmpty();
    }
};

namespace GitStatusParser
{

GitStatusParseResult parsePorcelainV2(const QByteArray &output);

} // namespace GitStatusParser

#endif // GITSTATUSPARSER_H
