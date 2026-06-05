#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QTreeView>
#include <QDockWidget>
#include "./editor/multieditor.h"
#include "./find/findreplacemanager.h"
#include "./tool/cpp/headersourcemanager.h"
#include "./tool/markdown/markdownpreviewer.h"

namespace Ui
{
class MainWindow;
}

class FileTypeFilterProxyModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void openFileByArg(const QString &fileName);
    MultiEditor *multiEditor();

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

    void onFileDraged(const QList<QUrl> &urls);
    void openFileInNewWindow(const QString &filePath);
    void onFileClicked(const QModelIndex &index);
    void updateEditActions(bool available);
    void updateWindowTitle(bool modified);
    void onSearchResultsChanged();

private:
    void createDirView();
    void createDockWindows();
    void setRootDirectory(const QString &dirPath);
    void refreshHighlights();
    void openFileAndSetRootDirectory(const QString &fileName);
    void updateToolActions();
    MultiEditor *activeMultiEditor() const;
    QList<MultiEditor *> editorPanes() const;
    void connectMultiEditor(MultiEditor *multiEditor);
    void setActiveMultiEditor(MultiEditor *multiEditor);
    void setSplitViewEnabled(bool enabled);
    void syncOpenedFileToOtherPanes(MultiEditor *source, const QString &filePath);
    void syncClosedFileToOtherPanes(MultiEditor *source, const QString &filePath);
    void syncRenamedFileToOtherPanes(MultiEditor *source, const QString &oldFilePath, const QString &newFilePath);
    void showSameFileInRightPane();
    bool closeAllEditorPanes();
    void reapplyThemeToEditorPanes();
    void refreshVisibleMarkdownPreviews();

    Ui::MainWindow *ui;
    QFileSystemModel *m_pFileModel;
    FileTypeFilterProxyModel *m_fileFilterProxy;
    QTreeView *m_pTreeView;
    QDockWidget *m_pDock;
    MultiEditor *m_activeMultiEditor;
    bool m_syncingEditorPanes;
    bool m_singleFileWindow;
    FindReplaceManager *m_findReplaceManager;
    HeaderSourceManager *m_headerSourceManager;
    MarkdownPreviewer *m_markdownPreviewer;
};

#endif // MAINWINDOW_H
