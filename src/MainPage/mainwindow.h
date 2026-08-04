#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include "find/findreplacemanager.h"
#include "tool/cpp/headersourcemanager.h"
#include "tool/markdown/markdownpreviewer.h"

namespace Ui
{
class MainWindow;
}

class QAction;
class QMenu;
class CustomEditor;
class DocumentController;
class DocumentSaveCoordinator;
class ExternalFileWatcher;
class FileExplorerDock;
class SearchResultsDock;
class SplitEditorCoordinator;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr,
                        const QStringList &initialFiles = {},
                        bool singleFileWindow = false);
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
    void on_action_findInFiles_triggered();
    void on_action_foldCurrentBlock_triggered();
    void on_action_toggleAllFolds_triggered();
    void on_action_goToMatchingBrace_triggered();
    void on_action_completeWord_triggered();
    void on_action_formatCode_triggered();
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
    // Builds Tools ▸ Encoding.  Called before the first updateToolActions().
    void setupEncodingMenu();
    void setRootDirectory(const QString &dirPath, bool explicitDirectoryRoot = false);
    void syncFileExplorerToFile(const QString &filePath, bool allowImplicitRootChange);
    bool isPathInExplorerRoot(const QString &filePath) const;
    void refreshHighlights();
    void openFileAndSetRootDirectory(const QString &fileName);
    void updateToolActions();
    // Toggle the Markdown side preview for the given pane (used by both the
    // Tools menu action and each pane's side-preview toolbar button).
    void toggleMarkdownSidePreviewFor(DocumentController *controller);
    // Re-apply font preferences to every open editor and refresh visible
    // Markdown previews.  Does not touch the file-tree dock (that setting only
    // seeds the next launch) or reopen documents.
    void applyPreferences();
    // Refreshes all palette-derived chrome without allowing stylesheet
    // repolishing to recursively start another refresh.
    void reapplyChromeTheme();
    // Qt may announce a color-scheme change before its application palette is
    // finalized. Coalesce refreshes and run them on the next event-loop turn.
    void scheduleChromeThemeRefresh();
    DocumentController *activeController() const;
    void applyActiveController(DocumentController *controller);
    // Opens the file (if needed) and moves the cursor to the given match,
    // selecting it. Loads are asynchronous, so the jump may be deferred until
    // the controller reports the file as opened.
    void openFileAtSearchMatch(const QString &filePath, int line, int column, int length);
    void jumpToEditorLocation(CustomEditor *editor, int line, int column, int length);

    struct PendingLocationJump
    {
        QString filePath;
        int line = 0;
        int column = 0;
        int length = 0;
        bool active = false;
    };

    Ui::MainWindow *ui;
    FileExplorerDock *m_fileExplorerDock;
    SearchResultsDock *m_searchResultsDock = nullptr;
    PendingLocationJump m_pendingSearchJump;
    QMenu *m_encodingMenu = nullptr;
    QMenu *m_reopenEncodingMenu = nullptr;
    QMenu *m_convertEncodingMenu = nullptr;
    QList<QAction *> m_convertEncodingActions;
    DocumentSaveCoordinator *m_saveCoordinator;
    ExternalFileWatcher *m_externalFileWatcher;
    // One controller per editor pane, in pane order (the .ui currently
    // provides two). Everything downstream iterates the list instead of
    // assuming a left/right pair.
    QList<DocumentController *> m_documentControllers;
    SplitEditorCoordinator *m_editorCoordinator;
    bool m_singleFileWindow;
    QString m_fileExplorerRootDirectory;
    bool m_fileExplorerRootExplicit;
    bool m_reapplyingChromeTheme = false;
    bool m_chromeThemeRefreshPending = false;
    FindReplaceManager *m_findReplaceManager;
    HeaderSourceManager *m_headerSourceManager;
    MarkdownPreviewer *m_markdownPreviewer;
};

#endif // MAINWINDOW_H
