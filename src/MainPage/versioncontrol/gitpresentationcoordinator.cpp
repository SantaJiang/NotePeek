#include "versioncontrol/gitpresentationcoordinator.h"

#include "editor/externalfilewatcher.h"
#include "versioncontrol/gitrepositoryservice.h"
#include "widgets/fileexplorerdock.h"
#include "widgets/qscieditorinfostatusbar.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPointer>

namespace {

bool hasGitMetadataAtOrAbove(const QString &directory)
{
    if (directory.isEmpty())
        return false;

    QDir candidate(directory);
    while (candidate.exists())
    {
        if (QFileInfo(candidate.filePath(QStringLiteral(".git"))).exists())
            return true;
        if (!candidate.cdUp())
            break;
    }
    return false;
}

} // namespace

GitPresentationCoordinator::GitPresentationCoordinator(
    FileExplorerDock *fileExplorer,
    QsciEditorInfoStatusBar *statusBar,
    ExternalFileWatcher *externalFileWatcher,
    QObject *parent)
    : QObject(parent)
    , m_fileExplorer(fileExplorer)
    , m_statusBar(statusBar)
    , m_externalFileWatcher(externalFileWatcher)
{
}

GitPresentationCoordinator::~GitPresentationCoordinator()
{
    setEnabled(false);
}

void GitPresentationCoordinator::setEnabled(bool enabled)
{
    if (enabled)
    {
        if (m_service)
            return;

        m_service = new GitRepositoryService(this);
        GitRepositoryService *service = m_service;
        const QPointer<GitRepositoryService> serviceGuard(service);
        connect(m_fileExplorer, &FileExplorerDock::repositoryContentsChanged,
                service, &GitRepositoryService::requestRefresh);
        connect(m_statusBar, &QsciEditorInfoStatusBar::gitRefreshRequested,
                service, &GitRepositoryService::refreshNow);
        connect(service, &GitRepositoryService::snapshotChanged,
                this, [this, serviceGuard](const GitRepositorySnapshot &snapshot) {
                    if (m_service == serviceGuard.data())
                        updatePresentation(snapshot);
                });
        connect(service, &GitRepositoryService::stateChanged,
                this, [this, serviceGuard](GitRepositoryState state) {
                    if (m_service != serviceGuard.data())
                        return;
                    if (state != GitRepositoryState::Ready)
                        clearPresentation();
                });
        connect(m_externalFileWatcher, &ExternalFileWatcher::externalFileChanged,
                service, &GitRepositoryService::requestRefresh);
        service->setDirectory(m_directory);
        return;
    }

    delete m_service;
    m_service = nullptr;
    clearPresentation();
}

void GitPresentationCoordinator::setDirectory(const QString &directory)
{
    m_directory = directory;
    if (m_service)
        m_service->setDirectory(m_directory);
}

void GitPresentationCoordinator::requestRefresh()
{
    if (m_service)
        m_service->requestRefresh();
}

void GitPresentationCoordinator::handleWindowActivated()
{
    if (!m_service)
        return;

    if (m_service->state() == GitRepositoryState::NotRepository
        && hasGitMetadataAtOrAbove(m_directory))
    {
        m_service->setDirectory(m_directory);
    }
    else
    {
        m_service->requestRefresh();
    }
}

void GitPresentationCoordinator::updatePresentation(const GitRepositorySnapshot &snapshot)
{
    // A queued snapshot can outlive a service that was just disabled.
    if (!m_service)
        return;

    m_fileExplorer->setGitRepositorySnapshot(snapshot);

    QString branch = snapshot.branchName;
    if (snapshot.detachedHead)
        branch = QStringLiteral("@%1").arg(snapshot.headOid.left(8));
    if (branch.isEmpty())
        branch = QStringLiteral("Git");

    QString text = branch;
    if (snapshot.ahead > 0)
        text += QStringLiteral("  ↑%1").arg(snapshot.ahead);
    if (snapshot.behind > 0)
        text += QStringLiteral("  ↓%1").arg(snapshot.behind);
    if (!snapshot.files.isEmpty())
        text += QStringLiteral("  •%1").arg(snapshot.files.size());

    // Keep the established translation context after moving this behavior out
    // of MainWindow. The direct call also lets lupdate retain the translation.
    // The status bar shows this text in a click popup, not a hover tooltip.
    const QString details = QCoreApplication::translate(
                                "MainWindow",
                                "Repository: %1\nBranch: %2\nChanges: %3\n"
                                "Ahead: %4, Behind: %5")
                                .arg(snapshot.repositoryRoot, branch)
                                .arg(snapshot.files.size())
                                .arg(snapshot.ahead)
                                .arg(snapshot.behind);
    m_statusBar->setGitStatus(text, details);
}

void GitPresentationCoordinator::clearPresentation()
{
    m_statusBar->setGitStatus(QString());
    m_fileExplorer->setGitRepositorySnapshot(GitRepositorySnapshot{});
}
