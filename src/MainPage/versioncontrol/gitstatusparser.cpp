#include "versioncontrol/gitstatusparser.h"

#include <QList>
#include <QStringList>

namespace
{

bool splitPrefixFields(const QByteArray &record, int fieldCount, QList<QByteArray> *fields, QByteArray *remainder)
{
    fields->clear();
    qsizetype position = 0;
    for (int fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex)
    {
        while (position < record.size() && record.at(position) == ' ')
            ++position;
        if (position >= record.size())
            return false;

        const qsizetype fieldStart = position;
        while (position < record.size() && record.at(position) != ' ')
            ++position;
        fields->append(record.mid(fieldStart, position - fieldStart));
    }

    if (position >= record.size() || record.at(position) != ' ')
        return false;
    ++position;
    if (position >= record.size())
        return false;

    *remainder = record.mid(position);
    return true;
}

bool parseStatusPair(const QByteArray &field, QChar *indexStatus, QChar *workTreeStatus)
{
    if (field.size() != 2)
        return false;
    *indexStatus = QChar::fromLatin1(field.at(0));
    *workTreeStatus = QChar::fromLatin1(field.at(1));
    return true;
}

QString decodedPath(const QByteArray &path)
{
    return QString::fromUtf8(path);
}

QString malformedRecordError(const QByteArray &record)
{
    return QStringLiteral("Malformed Git status record: %1").arg(QString::fromUtf8(record.left(160)));
}

bool parseSignedCount(const QByteArray &field, char prefix, int *value)
{
    if (field.size() < 2 || field.at(0) != prefix)
        return false;
    bool ok = false;
    const int parsed = field.mid(1).toInt(&ok);
    if (!ok || parsed < 0)
        return false;
    *value = parsed;
    return true;
}

bool isKnownHeader(const QByteArray &key)
{
    return key == "branch.oid" || key == "branch.head" || key == "branch.upstream" || key == "branch.ab" ||
           key == "stash";
}

} // namespace

namespace GitStatusParser
{

GitStatusParseResult parsePorcelainV2(const QByteArray &output)
{
    GitStatusParseResult result;
    const QList<QByteArray> records = output.split('\0');
    for (qsizetype recordIndex = 0; recordIndex < records.size(); ++recordIndex)
    {
        const QByteArray &record = records.at(recordIndex);
        if (record.isEmpty())
            continue;

        if (record.startsWith("# "))
        {
            const qsizetype separator = record.indexOf(' ', 2);
            if (separator < 0)
            {
                const QByteArray key = record.mid(2);
                if (isKnownHeader(key))
                {
                    result.error = malformedRecordError(record);
                    return result;
                }
                continue;
            }

            const QByteArray key = record.mid(2, separator - 2);
            const QByteArray value = record.mid(separator + 1);
            if (key == "branch.oid")
            {
                if (value == "(initial)")
                {
                    result.snapshot.initialCommit = true;
                    result.snapshot.headOid.clear();
                }
                else
                {
                    result.snapshot.headOid = QString::fromLatin1(value);
                }
            }
            else if (key == "branch.head")
            {
                result.snapshot.detachedHead = value == "(detached)";
                result.snapshot.branchName = result.snapshot.detachedHead ? QString() : QString::fromUtf8(value);
            }
            else if (key == "branch.upstream")
            {
                result.snapshot.upstreamBranch = QString::fromUtf8(value);
            }
            else if (key == "branch.ab")
            {
                const QList<QByteArray> counts = value.split(' ');
                if (counts.size() != 2 || !parseSignedCount(counts.at(0), '+', &result.snapshot.ahead) ||
                    !parseSignedCount(counts.at(1), '-', &result.snapshot.behind))
                {
                    result.error = malformedRecordError(record);
                    return result;
                }
            }
            else if (key == "stash")
            {
                bool ok = false;
                const int stashCount = value.toInt(&ok);
                if (!ok || stashCount < 0)
                {
                    result.error = malformedRecordError(record);
                    return result;
                }
                result.snapshot.stashCount = stashCount;
            }
            // Porcelain v2 explicitly permits additional headers. Unknown
            // headers must be ignored for forward compatibility.
            continue;
        }

        GitFileStatus status;
        QList<QByteArray> fields;
        QByteArray path;
        switch (record.at(0))
        {
        case '1':
            if (!splitPrefixFields(record, 8, &fields, &path) ||
                !parseStatusPair(fields.at(1), &status.indexStatus, &status.workTreeStatus))
            {
                result.error = malformedRecordError(record);
                return result;
            }
            status.recordType = GitChangeRecordType::Ordinary;
            status.submoduleState = QString::fromLatin1(fields.at(2));
            status.path = decodedPath(path);
            break;
        case '2':
            if (!splitPrefixFields(record, 9, &fields, &path) ||
                !parseStatusPair(fields.at(1), &status.indexStatus, &status.workTreeStatus) ||
                recordIndex + 1 >= records.size() || records.at(recordIndex + 1).isEmpty())
            {
                result.error = malformedRecordError(record);
                return result;
            }
            status.recordType = GitChangeRecordType::RenamedOrCopied;
            status.submoduleState = QString::fromLatin1(fields.at(2));
            status.renameOrCopyScore = QString::fromLatin1(fields.at(8));
            status.path = decodedPath(path);
            status.originalPath = decodedPath(records.at(++recordIndex));
            break;
        case 'u':
            if (!splitPrefixFields(record, 10, &fields, &path) ||
                !parseStatusPair(fields.at(1), &status.indexStatus, &status.workTreeStatus))
            {
                result.error = malformedRecordError(record);
                return result;
            }
            status.recordType = GitChangeRecordType::Unmerged;
            status.submoduleState = QString::fromLatin1(fields.at(2));
            status.path = decodedPath(path);
            break;
        case '?':
        case '!':
            if (record.size() < 3 || record.at(1) != ' ')
            {
                result.error = malformedRecordError(record);
                return result;
            }
            status.recordType = record.at(0) == '?' ? GitChangeRecordType::Untracked : GitChangeRecordType::Ignored;
            status.path = decodedPath(record.mid(2));
            break;
        default:
            // A future Git version may add record types. Ignoring unknown
            // records is safer than disabling every known status entry.
            continue;
        }

        result.snapshot.files.append(status);
    }

    return result;
}

} // namespace GitStatusParser
