#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileSystemModel>
#include <QTreeView>
#include <QDockWidget>
#include "multieditor.h"

namespace Ui
{
class MainWindow;
}

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

    void onFileClicked(const QModelIndex &index);
    void updateEditActions(bool available);
    void updateWindowTitle(bool modified);

private:
    void createDirView();
    void createDockWindows();
    void setRootDirectory(const QString &dirPath);
    void refreshHighlights();

    Ui::MainWindow *ui;
    QFileSystemModel *m_pFileModel;
    QTreeView *m_pTreeView;
    QDockWidget *m_pDock;
};

#endif // MAINWINDOW_H
