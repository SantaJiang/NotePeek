#include "versioncontrol/gitexecutable.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

namespace
{

bool isUsableExecutable(const QString &path)
{
    if (path.isEmpty())
        return false;

    const QFileInfo info(path);
#ifdef Q_OS_WIN
    return info.exists() && info.isFile();
#else
    return info.exists() && info.isFile() && info.isExecutable();
#endif
}

QString normalizedExecutable(const QString &path)
{
    if (!isUsableExecutable(path))
        return QString();

    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

#ifdef Q_OS_MACOS
QString selectedDeveloperGit()
{
    const QString selectedGit = QStringLiteral("/var/db/xcode_select_link/usr/bin/git");
    if (isUsableExecutable(selectedGit))
        return normalizedExecutable(selectedGit);

    const QString commandLineToolsGit = QStringLiteral("/Library/Developer/CommandLineTools/usr/bin/git");
    if (isUsableExecutable(commandLineToolsGit))
        return normalizedExecutable(commandLineToolsGit);

    const QString defaultXcodeGit = QStringLiteral("/Applications/Xcode.app/Contents/Developer/usr/bin/git");
    if (isUsableExecutable(defaultXcodeGit))
        return normalizedExecutable(defaultXcodeGit);

    return QString();
}
#endif

#ifdef Q_OS_WIN
QString findGitForWindows()
{
    const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QStringList bases = {
        environment.value(QStringLiteral("ProgramW6432")), environment.value(QStringLiteral("ProgramFiles")),
        environment.value(QStringLiteral("ProgramFiles(x86)")), environment.value(QStringLiteral("LocalAppData"))};
    for (const QString &base : bases)
    {
        if (base.isEmpty())
            continue;

        const QStringList candidates = {QDir(base).filePath(QStringLiteral("Git/cmd/git.exe")),
                                        QDir(base).filePath(QStringLiteral("Programs/Git/cmd/git.exe"))};
        for (const QString &candidate : candidates)
        {
            if (isUsableExecutable(candidate))
                return normalizedExecutable(candidate);
        }
    }
    return QString();
}
#endif

} // namespace

namespace GitExecutable
{

QString find(const QString &configuredPath)
{
    if (!configuredPath.isEmpty())
        return normalizedExecutable(configuredPath);

    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("git"));
#ifdef Q_OS_MACOS
    // /usr/bin/git is an Apple shim that may open an installation prompt when
    // Command Line Tools are absent. Resolve the selected developer toolchain
    // directly so passive repository discovery never causes that prompt.
    if (fromPath == QStringLiteral("/usr/bin/git"))
        return selectedDeveloperGit();
#endif
    if (isUsableExecutable(fromPath))
        return normalizedExecutable(fromPath);

#ifdef Q_OS_WIN
    return findGitForWindows();
#else
    return QString();
#endif
}

GitVersion parseVersion(const QByteArray &versionOutput)
{
    GitVersion version;
    const QString display = QString::fromUtf8(versionOutput).trimmed();
    static const QRegularExpression expression(QStringLiteral(R"((?:^|\s)(\d+)\.(\d+)(?:\.(\d+))?)"));
    const QRegularExpressionMatch match = expression.match(display);
    if (!match.hasMatch())
        return version;

    bool majorOk = false;
    bool minorOk = false;
    bool patchOk = true;
    version.major = match.captured(1).toInt(&majorOk);
    version.minor = match.captured(2).toInt(&minorOk);
    version.patch = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt(&patchOk);
    if (!majorOk || !minorOk || !patchOk)
        return GitVersion{};

    version.displayString = display;
    return version;
}

} // namespace GitExecutable
