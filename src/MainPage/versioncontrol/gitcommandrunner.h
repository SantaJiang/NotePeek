#ifndef GITCOMMANDRUNNER_H
#define GITCOMMANDRUNNER_H

#include <QByteArray>
#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

class QProcess;

struct GitCommandOptions
{
    bool readOnly = true;
    bool disableTerminalPrompts = true;
    int timeoutMs = 10000;
};

struct GitCommandResult
{
    quint64 requestId = 0;
    QString workingDirectory;
    QStringList arguments;
    QByteArray standardOutput;
    QByteArray standardError;
    QString processError;
    int exitCode = -1;
    bool started = false;
    bool crashed = false;
    bool canceled = false;
    bool timedOut = false;

    bool succeeded() const
    {
        return started && !crashed && !canceled && !timedOut && processError.isEmpty() && exitCode == 0;
    }
};

Q_DECLARE_METATYPE(GitCommandResult)

class GitCommandRunner : public QObject
{
    Q_OBJECT

public:
    explicit GitCommandRunner(const QString &gitExecutable, QObject *parent = nullptr);
    ~GitCommandRunner() override;

    QString gitExecutable() const
    {
        return m_gitExecutable;
    }
    quint64 start(const QString &workingDirectory, const QStringList &arguments, const GitCommandOptions &options = {});
    void cancel(quint64 requestId);
    void cancelAll();

signals:
    void commandFinished(const GitCommandResult &result);

private:
    void finalize(QProcess *process, int exitCode, bool crashed);

    QString m_gitExecutable;
    quint64 m_nextRequestId = 1;
    QHash<quint64, QProcess *> m_processes;
};

#endif // GITCOMMANDRUNNER_H
