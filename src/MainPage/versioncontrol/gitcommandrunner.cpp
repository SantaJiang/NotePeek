#include "versioncontrol/gitcommandrunner.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QVariant>

namespace
{

constexpr auto RequestIdProperty = "notepeekGitRequestId";
constexpr auto WorkingDirectoryProperty = "notepeekGitWorkingDirectory";
constexpr auto ArgumentsProperty = "notepeekGitArguments";
constexpr auto StartedProperty = "notepeekGitStarted";
constexpr auto CanceledProperty = "notepeekGitCanceled";
constexpr auto TimedOutProperty = "notepeekGitTimedOut";
constexpr auto FinalizedProperty = "notepeekGitFinalized";
constexpr auto ProcessErrorProperty = "notepeekGitProcessError";

} // namespace

GitCommandRunner::GitCommandRunner(const QString &gitExecutable, QObject *parent)
    : QObject(parent)
    , m_gitExecutable(gitExecutable)
{
    qRegisterMetaType<GitCommandResult>();
}

GitCommandRunner::~GitCommandRunner()
{
    const QList<QProcess *> processes = m_processes.values();
    m_processes.clear();
    for (QProcess *process : processes)
    {
        if (!process)
            continue;
        process->disconnect(this);
        if (process->state() != QProcess::NotRunning)
        {
            process->kill();
            process->waitForFinished(1000);
        }
    }
}

quint64 GitCommandRunner::start(const QString &workingDirectory, const QStringList &arguments,
                                const GitCommandOptions &options)
{
    const quint64 requestId = m_nextRequestId++;
    auto *process = new QProcess(this);
    process->setProperty(RequestIdProperty, QVariant::fromValue(requestId));
    process->setProperty(WorkingDirectoryProperty, workingDirectory);
    process->setProperty(ArgumentsProperty, arguments);
    process->setProperty(StartedProperty, false);
    process->setProperty(CanceledProperty, false);
    process->setProperty(TimedOutProperty, false);
    process->setProperty(FinalizedProperty, false);
    if (!workingDirectory.isEmpty())
        process->setWorkingDirectory(workingDirectory);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QStringList repositoryEnvironmentVariables = {QStringLiteral("GIT_DIR"),
                                                        QStringLiteral("GIT_WORK_TREE"),
                                                        QStringLiteral("GIT_INDEX_FILE"),
                                                        QStringLiteral("GIT_OBJECT_DIRECTORY"),
                                                        QStringLiteral("GIT_ALTERNATE_OBJECT_DIRECTORIES"),
                                                        QStringLiteral("GIT_COMMON_DIR"),
                                                        QStringLiteral("GIT_PREFIX"),
                                                        QStringLiteral("GIT_CEILING_DIRECTORIES"),
                                                        QStringLiteral("GIT_DISCOVERY_ACROSS_FILESYSTEM"),
                                                        QStringLiteral("GIT_NAMESPACE"),
                                                        QStringLiteral("GIT_SHALLOW_FILE"),
                                                        QStringLiteral("GIT_QUARANTINE_PATH")};
    for (const QString &variable : repositoryEnvironmentVariables)
        environment.remove(variable);
    environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    environment.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    if (options.readOnly)
        environment.insert(QStringLiteral("GIT_OPTIONAL_LOCKS"), QStringLiteral("0"));
    if (options.disableTerminalPrompts)
        environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    process->setProcessEnvironment(environment);

    connect(process, &QProcess::started, this,
            [process]()
            {
                process->setProperty(StartedProperty, true);
                process->closeWriteChannel();
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error)
            {
                process->setProperty(ProcessErrorProperty, process->errorString());
                if (error == QProcess::FailedToStart)
                    finalize(process, -1, false);
            });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process](int exitCode, QProcess::ExitStatus exitStatus)
            { finalize(process, exitCode, exitStatus == QProcess::CrashExit); });

    if (options.timeoutMs > 0)
    {
        auto *timer = new QTimer(process);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this,
                [process]()
                {
                    if (process->state() == QProcess::NotRunning)
                        return;
                    process->setProperty(TimedOutProperty, true);
                    process->kill();
                });
        timer->start(options.timeoutMs);
    }

    m_processes.insert(requestId, process);
    if (m_gitExecutable.isEmpty())
    {
        process->setProperty(ProcessErrorProperty, QStringLiteral("Git executable was not found."));
        QTimer::singleShot(0, process, [this, process]() { finalize(process, -1, false); });
        return requestId;
    }

    process->start(m_gitExecutable, arguments);
    return requestId;
}

void GitCommandRunner::cancel(quint64 requestId)
{
    QProcess *process = m_processes.value(requestId);
    if (!process || process->property(FinalizedProperty).toBool())
        return;

    process->setProperty(CanceledProperty, true);
    if (process->state() == QProcess::NotRunning)
        finalize(process, -1, false);
    else
        process->kill();
}

void GitCommandRunner::cancelAll()
{
    const QList<quint64> requestIds = m_processes.keys();
    for (quint64 requestId : requestIds)
        cancel(requestId);
}

void GitCommandRunner::finalize(QProcess *process, int exitCode, bool crashed)
{
    if (!process || process->property(FinalizedProperty).toBool())
        return;

    process->setProperty(FinalizedProperty, true);
    const quint64 requestId = process->property(RequestIdProperty).toULongLong();
    m_processes.remove(requestId);

    GitCommandResult result;
    result.requestId = requestId;
    result.workingDirectory = process->property(WorkingDirectoryProperty).toString();
    result.arguments = process->property(ArgumentsProperty).toStringList();
    result.standardOutput = process->readAllStandardOutput();
    result.standardError = process->readAllStandardError();
    result.processError = process->property(ProcessErrorProperty).toString();
    result.exitCode = exitCode;
    result.started = process->property(StartedProperty).toBool();
    result.crashed = crashed;
    result.canceled = process->property(CanceledProperty).toBool();
    result.timedOut = process->property(TimedOutProperty).toBool();

    process->deleteLater();
    emit commandFinished(result);
}
