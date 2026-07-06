#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include "find/findreplacemanager.h"
#include "tool/cpp/headersourcemanager.h"
#include "tool/markdown/markdownpreviewer.h"

namespace Ui
{
class MainWindow;
}

class DocumentController;
class FileExplorerDock;
class SplitEditorCoordinator;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void openFileByArg(const QString &fileName);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void on_action_new_triggered();
    void on_action_openFile_triggered();
    void on_action_openDirectory_triggered();
    bool on_action_save_triggered();
    bool on_action_saveAs_triggered();
    void on_action_exit_triggered();
    void on_action_about_triggered();
    void on_action_showSlideBar_triggered(bool checked);
    void on_action_undo_triggered();
    void on_action_redo_triggered();
    void on_action_cut_triggered();
    void on_action_copy_triggered();
    void on_action_paste_triggered();
    void on_action_find_triggered();
    void on_action_switchHeaderSource_triggered();
    void on_action_markdownPreview_triggered();
    void on_action_markdownExportPdf_triggered();
    void on_action_preferences_triggered();

    void onFileDraged(const QList<QUrl> &urls);
    void openFileInNewWindow(const QString &filePath);
    void updateEditActions(bool available);
    void updateWindowTitle(bool modified);
    void onSearchResultsChanged();

private:
    void setRootDirectory(const QString &dirPath, bool explicitDirectoryRoot = false);
    void syncFileExplorerToFile(const QString &filePath, bool allowImplicitRootChange);
    bool isPathInExplorerRoot(const QString &filePath) const;
    void refreshHighlights();
    void openFileAndSetRootDirectory(const QString &fileName);
    void updateToolActions();
    // Re-apply font preferences to every open editor and refresh visible
    // Markdown previews.  Does not touch the file-tree dock (that setting only
    // seeds the next launch) or reopen documents.
    void applyPreferences();
    DocumentController *activeController() const;
    void applyActiveController(DocumentController *controller);

    Ui::MainWindow *ui;
    FileExplorerDock *m_fileExplorerDock;
    DocumentController *m_leftController;
    DocumentController *m_rightController;
    SplitEditorCoordinator *m_editorCoordinator;
    bool m_singleFileWindow;
    QString m_fileExplorerRootDirectory;
    bool m_fileExplorerRootExplicit;
    FindReplaceManager *m_findReplaceManager;
    HeaderSourceManager *m_headerSourceManager;
    MarkdownPreviewer *m_markdownPreviewer;
};

#endif // MAINWINDOW_H
