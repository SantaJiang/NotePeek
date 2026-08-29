#ifndef GITPRESENTATIONCOORDINATOR_H
#define GITPRESENTATIONCOORDINATOR_H

#include <QObject>
#include <QString>

#include "versioncontrol/gittypes.h"

class ExternalFileWatcher;
class FileExplorerDock;
class GitRepositoryService;
class QsciEditorInfoStatusBar;

// Owns the optional Git service and maps repository snapshots onto the existing
// file-tree and status-bar views. MainWindow only forwards application events.
class GitPresentationCoordinator final : public QObject
{
    Q_OBJECT

public:
    explicit GitPresentationCoordinator(FileExplorerDock *fileExplorer,
                                        QsciEditorInfoStatusBar *statusBar,
                                        ExternalFileWatcher *externalFileWatcher,
                                        QObject *parent = nullptr);
    ~GitPresentationCoordinator() override;

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_service != nullptr; }
    void setDirectory(const QString &directory);
    void requestRefresh();
    void handleWindowActivated();

private:
    void updatePresentation(const GitRepositorySnapshot &snapshot);
    void clearPresentation();

    FileExplorerDock *m_fileExplorer;
    QsciEditorInfoStatusBar *m_statusBar;
    ExternalFileWatcher *m_externalFileWatcher;
    GitRepositoryService *m_service = nullptr;
    QString m_directory;
};

#endif // GITPRESENTATIONCOORDINATOR_H
