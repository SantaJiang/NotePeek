#ifndef FILEEXPLORERDOCK_H
#define FILEEXPLORERDOCK_H

#include <QObject>
#include <QModelIndex>
#include <QString>

#include "versioncontrol/gittypes.h"

class FileTreeView;
class FileTypeFilterProxyModel;
class QDockWidget;
class QFileSystemModel;
class QWidget;

class FileExplorerDock : public QObject
{
    Q_OBJECT

public:
    explicit FileExplorerDock(QWidget *parent);

    QDockWidget *dockWidget() const;
    void setVisible(bool visible);
    void reapplyTheme();
    void setRootDirectory(const QString &dirPath);
    void setGitRepositorySnapshot(const GitRepositorySnapshot &snapshot);
    void selectFile(const QString &filePath);
    void clearSelection();
    void expandDirectory(const QString &dirPath);

signals:
    void fileActivated(const QString &filePath);
    void openInNewWindowRequested(const QString &filePath);
    void repositoryContentsChanged();

private slots:
    void onTreeDoubleClicked(const QModelIndex &index);

private:
    QModelIndex proxyIndexForPath(const QString &path) const;

    QFileSystemModel *m_fileModel;
    FileTypeFilterProxyModel *m_fileFilterProxy;
    FileTreeView *m_treeView;
    QDockWidget *m_dock;
};

#endif // FILEEXPLORERDOCK_H
