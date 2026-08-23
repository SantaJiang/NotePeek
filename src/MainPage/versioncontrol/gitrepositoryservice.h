#ifndef GITREPOSITORYSERVICE_H
#define GITREPOSITORYSERVICE_H

#include "versioncontrol/gittypes.h"

#include <QObject>
#include <QString>

class GitCommandRunner;
class QTimer;
struct GitCommandResult;

class GitRepositoryService : public QObject
{
    Q_OBJECT

public:
    explicit GitRepositoryService(QObject *parent = nullptr);
    explicit GitRepositoryService(const QString &gitExecutable, QObject *parent = nullptr);
    ~GitRepositoryService() override;

    static GitVersion minimumSupportedVersion();

    void setDirectory(const QString &directory);
    void requestRefresh();
    void refreshNow();
    void cancel();

    QString directory() const
    {
        return m_directory;
    }
    QString repositoryRoot() const
    {
        return m_repositoryRoot;
    }
    GitInstallation installation() const
    {
        return m_installation;
    }
    GitRepositoryState state() const
    {
        return m_state;
    }
    GitRepositorySnapshot snapshot() const
    {
        return m_snapshot;
    }
    bool isRefreshing() const
    {
        return m_statusInFlight;
    }

signals:
    void installationChanged(const GitInstallation &installation);
    void stateChanged(GitRepositoryState state);
    void repositoryRootChanged(const QString &repositoryRoot);
    void refreshingChanged(bool refreshing);
    void snapshotChanged(const GitRepositorySnapshot &snapshot);
    void errorOccurred(const QString &message);

private:
    enum class Operation
    {
        None,
        Version,
        Discover,
        Status
    };

    void setState(GitRepositoryState state);
    void setRepositoryRoot(const QString &repositoryRoot);
    void startVersionCheck();
    void startRepositoryDiscovery();
    void startStatusRefresh();
    void handleCommandFinished(const GitCommandResult &result);
    void finishStatusRefresh();
    void reportError(const QString &context, const GitCommandResult &result);

    GitCommandRunner *m_runner;
    QTimer *m_refreshTimer;
    QString m_directory;
    QString m_repositoryRoot;
    GitInstallation m_installation;
    GitRepositoryState m_state = GitRepositoryState::Idle;
    GitRepositorySnapshot m_snapshot;
    Operation m_operation = Operation::None;
    quint64 m_activeRequestId = 0;
    quint64 m_generation = 0;
    quint64 m_activeGeneration = 0;
    bool m_statusInFlight = false;
    bool m_refreshPending = false;
};

#endif // GITREPOSITORYSERVICE_H
