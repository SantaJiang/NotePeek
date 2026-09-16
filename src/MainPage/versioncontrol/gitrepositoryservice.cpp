#include "versioncontrol/gitrepositoryservice.h"

#include "versioncontrol/gitcommandrunner.h"
#include "versioncontrol/gitexecutable.h"
#include "versioncontrol/gitstatusparser.h"

#include <QDir>
#include <QFileInfo>
#include <QTimer>

namespace
{

constexpr int RefreshDebounceMs = 300;
constexpr int DiscoveryTimeoutMs = 15000;
constexpr int StatusTimeoutMs = 15000;

QString normalizedDirectory(const QString &path)
{
    if (path.isEmpty())
        return QString();

    const QFileInfo info(path);
    const QFileInfo directoryInfo = info.isFile() ? QFileInfo(info.absolutePath()) : info;
    const QString canonical = directoryInfo.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? directoryInfo.absoluteFilePath() : canonical);
}

QString repositoryRootFromOutput(QByteArray output)
{
    while (output.endsWith('\n') || output.endsWith('\r'))
        output.chop(1);
    return normalizedDirectory(QString::fromUtf8(output));
}

QString commandFailureMessage(const GitCommandResult &result)
{
    if (result.timedOut)
        return QStringLiteral("Git command timed out.");
    if (!result.processError.isEmpty())
        return result.processError;

    const QString standardError = QString::fromUtf8(result.standardError).trimmed();
    if (!standardError.isEmpty())
        return standardError;
    return QStringLiteral("Git exited with code %1.").arg(result.exitCode);
}

bool isNotRepositoryResult(const GitCommandResult &result)
{
    if (!result.started || result.crashed || result.timedOut || !result.processError.isEmpty())
    {
        return false;
    }
    return result.standardError.contains("not a git repository") || result.standardError.contains("outside repository");
}

} // namespace

GitRepositoryService::GitRepositoryService(QObject *parent)
    : GitRepositoryService(GitExecutable::find(), parent)
{
}

GitRepositoryService::GitRepositoryService(const QString &gitExecutable, QObject *parent)
    : QObject(parent)
    , m_runner(new GitCommandRunner(gitExecutable, this))
    , m_refreshTimer(new QTimer(this))
{
    qRegisterMetaType<GitInstallation>();
    qRegisterMetaType<GitRepositoryState>();
    qRegisterMetaType<GitRepositorySnapshot>();

    m_installation.executablePath = gitExecutable;
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(RefreshDebounceMs);
    connect(m_refreshTimer, &QTimer::timeout, this, &GitRepositoryService::startStatusRefresh);
    connect(m_runner, &GitCommandRunner::commandFinished, this, &GitRepositoryService::handleCommandFinished);
}

GitRepositoryService::~GitRepositoryService()
{
    cancel();
}

GitVersion GitRepositoryService::minimumSupportedVersion()
{
    return GitVersion{2, 23, 0, QStringLiteral("2.23.0")};
}

void GitRepositoryService::setDirectory(const QString &directory)
{
    const QString normalized = normalizedDirectory(directory);
    if (normalized == m_directory && m_state == GitRepositoryState::Ready && QFileInfo(normalized).isDir())
    {
        requestRefresh();
        return;
    }

    cancel();
    ++m_generation;
    m_directory = normalized;
    m_snapshot = GitRepositorySnapshot{};
    setRepositoryRoot(QString());

    if (m_directory.isEmpty() || !QFileInfo(m_directory).isDir())
    {
        setState(GitRepositoryState::Idle);
        return;
    }

    if (m_installation.isAvailable())
        startRepositoryDiscovery();
    else
        startVersionCheck();
}

void GitRepositoryService::requestRefresh()
{
    if (m_repositoryRoot.isEmpty() || m_state != GitRepositoryState::Ready)
        return;

    if (m_statusInFlight)
    {
        m_refreshPending = true;
        return;
    }
    m_refreshTimer->start();
}

void GitRepositoryService::refreshNow()
{
    if (m_repositoryRoot.isEmpty() || m_state != GitRepositoryState::Ready)
        return;
    m_refreshTimer->stop();
    startStatusRefresh();
}

void GitRepositoryService::cancel()
{
    const Operation canceledOperation = m_operation;
    m_refreshTimer->stop();
    m_refreshPending = false;
    if (m_activeRequestId != 0)
        m_runner->cancel(m_activeRequestId);
    m_activeRequestId = 0;
    m_operation = Operation::None;
    if (m_statusInFlight)
    {
        m_statusInFlight = false;
        emit refreshingChanged(false);
    }
    if (canceledOperation == Operation::Version || canceledOperation == Operation::Discover)
    {
        setState(GitRepositoryState::Idle);
    }
}

void GitRepositoryService::setState(GitRepositoryState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void GitRepositoryService::setRepositoryRoot(const QString &repositoryRoot)
{
    if (m_repositoryRoot == repositoryRoot)
        return;
    m_repositoryRoot = repositoryRoot;
    emit repositoryRootChanged(repositoryRoot);
}

void GitRepositoryService::startVersionCheck()
{
    setState(GitRepositoryState::CheckingGit);
    m_operation = Operation::Version;
    m_activeGeneration = m_generation;
    GitCommandOptions options;
    options.timeoutMs = DiscoveryTimeoutMs;
    m_activeRequestId = m_runner->start(m_directory, {QStringLiteral("--version")}, options);
}

void GitRepositoryService::startRepositoryDiscovery()
{
    setState(GitRepositoryState::DiscoveringRepository);
    m_operation = Operation::Discover;
    m_activeGeneration = m_generation;
    GitCommandOptions options;
    options.timeoutMs = DiscoveryTimeoutMs;
    m_activeRequestId =
        m_runner->start(m_directory, {QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")}, options);
}

void GitRepositoryService::startStatusRefresh()
{
    if (m_statusInFlight || m_repositoryRoot.isEmpty() || m_state != GitRepositoryState::Ready)
    {
        if (m_statusInFlight)
            m_refreshPending = true;
        return;
    }

    m_statusInFlight = true;
    emit refreshingChanged(true);
    m_operation = Operation::Status;
    m_activeGeneration = m_generation;
    GitCommandOptions options;
    options.timeoutMs = StatusTimeoutMs;
    m_activeRequestId = m_runner->start(m_repositoryRoot,
                                        {QStringLiteral("status"), QStringLiteral("--porcelain=v2"),
                                         QStringLiteral("-z"), QStringLiteral("--branch"),
                                         QStringLiteral("--show-stash"), QStringLiteral("--untracked-files=normal")},
                                        options);
}

void GitRepositoryService::handleCommandFinished(const GitCommandResult &result)
{
    if (result.requestId != m_activeRequestId)
        return;

    const Operation completedOperation = m_operation;
    const bool stale = m_activeGeneration != m_generation;
    m_activeRequestId = 0;
    m_operation = Operation::None;
    if (stale || result.canceled)
    {
        if (completedOperation == Operation::Status)
            finishStatusRefresh();
        return;
    }

    if (completedOperation == Operation::Version)
    {
        m_installation.error.clear();
        if (!result.succeeded())
        {
            m_installation.version = GitVersion{};
            m_installation.error = commandFailureMessage(result);
            emit installationChanged(m_installation);
            setState(GitRepositoryState::GitUnavailable);
            return;
        }

        m_installation.version = GitExecutable::parseVersion(result.standardOutput);
        if (!m_installation.version.isValid())
        {
            m_installation.error = QStringLiteral("Git returned an unrecognized version string.");
            emit installationChanged(m_installation);
            setState(GitRepositoryState::GitUnavailable);
            return;
        }
        if (m_installation.version < minimumSupportedVersion())
        {
            m_installation.error = QStringLiteral("Git 2.23.0 or newer is required.");
            emit installationChanged(m_installation);
            setState(GitRepositoryState::UnsupportedGitVersion);
            return;
        }

        emit installationChanged(m_installation);
        startRepositoryDiscovery();
        return;
    }

    if (completedOperation == Operation::Discover)
    {
        if (!result.succeeded())
        {
            setRepositoryRoot(QString());
            if (isNotRepositoryResult(result))
            {
                setState(GitRepositoryState::NotRepository);
            }
            else
            {
                setState(GitRepositoryState::Error);
                reportError(QStringLiteral("Repository discovery"), result);
            }
            return;
        }

        const QString root = repositoryRootFromOutput(result.standardOutput);
        if (root.isEmpty() || !QFileInfo(root).isDir())
        {
            setState(GitRepositoryState::Error);
            emit errorOccurred(QStringLiteral("Repository discovery returned an invalid root directory."));
            return;
        }

        setRepositoryRoot(root);
        setState(GitRepositoryState::Ready);
        startStatusRefresh();
        return;
    }

    if (completedOperation == Operation::Status)
    {
        if (!result.succeeded())
        {
            if (isNotRepositoryResult(result))
            {
                m_snapshot = GitRepositorySnapshot{};
                setRepositoryRoot(QString());
                setState(GitRepositoryState::NotRepository);
                finishStatusRefresh();
                return;
            }
            reportError(QStringLiteral("Git status"), result);
            finishStatusRefresh();
            return;
        }

        GitStatusParseResult parsed = GitStatusParser::parsePorcelainV2(result.standardOutput);
        if (!parsed.succeeded())
        {
            emit errorOccurred(parsed.error);
            finishStatusRefresh();
            return;
        }

        parsed.snapshot.repositoryRoot = m_repositoryRoot;
        parsed.snapshot.revision = m_snapshot.revision + 1;
        m_snapshot = parsed.snapshot;
        emit snapshotChanged(m_snapshot);
        finishStatusRefresh();
    }
}

void GitRepositoryService::finishStatusRefresh()
{
    if (m_statusInFlight)
    {
        m_statusInFlight = false;
        emit refreshingChanged(false);
    }

    if (m_refreshPending)
    {
        m_refreshPending = false;
        if (m_state == GitRepositoryState::Ready)
            m_refreshTimer->start(0);
    }
}

void GitRepositoryService::reportError(const QString &context, const GitCommandResult &result)
{
    emit errorOccurred(QStringLiteral("%1: %2").arg(context, commandFailureMessage(result)));
}
