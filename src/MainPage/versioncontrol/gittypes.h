#ifndef GITTYPES_H
#define GITTYPES_H

#include <QList>
#include <QMetaType>
#include <QString>

struct GitVersion
{
    int major = -1;
    int minor = -1;
    int patch = -1;
    QString displayString;

    bool isValid() const
    {
        return major >= 0 && minor >= 0 && patch >= 0;
    }
};

inline bool operator==(const GitVersion &left, const GitVersion &right)
{
    return left.major == right.major && left.minor == right.minor && left.patch == right.patch &&
           left.displayString == right.displayString;
}

inline bool operator<(const GitVersion &left, const GitVersion &right)
{
    if (left.major != right.major)
        return left.major < right.major;
    if (left.minor != right.minor)
        return left.minor < right.minor;
    return left.patch < right.patch;
}

enum class GitChangeRecordType
{
    Ordinary,
    RenamedOrCopied,
    Unmerged,
    Untracked,
    Ignored
};

struct GitFileStatus
{
    GitChangeRecordType recordType = GitChangeRecordType::Ordinary;
    QChar indexStatus = QLatin1Char('.');
    QChar workTreeStatus = QLatin1Char('.');
    QString path;
    QString originalPath;
    QString submoduleState;
    QString renameOrCopyScore;

    bool isConflict() const
    {
        return recordType == GitChangeRecordType::Unmerged;
    }

    bool isUntracked() const
    {
        return recordType == GitChangeRecordType::Untracked;
    }

    bool isIgnored() const
    {
        return recordType == GitChangeRecordType::Ignored;
    }
};

struct GitRepositorySnapshot
{
    QString repositoryRoot;
    QString headOid;
    QString branchName;
    QString upstreamBranch;
    int ahead = 0;
    int behind = 0;
    int stashCount = 0;
    bool initialCommit = false;
    bool detachedHead = false;
    quint64 revision = 0;
    QList<GitFileStatus> files;
};

struct GitInstallation
{
    QString executablePath;
    GitVersion version;
    QString error;

    bool isAvailable() const
    {
        return !executablePath.isEmpty() && version.isValid() && error.isEmpty();
    }
};

enum class GitRepositoryState
{
    Idle,
    CheckingGit,
    DiscoveringRepository,
    NotRepository,
    Ready,
    GitUnavailable,
    UnsupportedGitVersion,
    Error
};

Q_DECLARE_METATYPE(GitVersion)
Q_DECLARE_METATYPE(GitFileStatus)
Q_DECLARE_METATYPE(GitRepositorySnapshot)
Q_DECLARE_METATYPE(GitInstallation)
Q_DECLARE_METATYPE(GitRepositoryState)

#endif // GITTYPES_H
