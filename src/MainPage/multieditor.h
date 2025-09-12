#ifndef MULTIEDITOR_H
#define MULTIEDITOR_H

#include <QWidget>
#include <QTabWidget>
#include <QMap>
#include <QFileInfo>
#include <QTextStream>
#include <QMessageBox>
#include <QApplication>
#include <QFontDatabase>
#include <QCloseEvent>
#include <QVBoxLayout>
#include "editormanager.h"
#include "editorswitchpopup.h"

namespace Ui
{
class MultiEditor;
}

class QsciScintilla;
class MultiEditor : public QWidget
{
    Q_OBJECT
public:
    explicit MultiEditor(QWidget *parent = nullptr, bool createDefault = true);
    ~MultiEditor();

    // 编辑器管理..
    QsciScintilla *currentEditor() const;
    int findEditorIndex(QsciScintilla *editor) const;
    void switchToEditor(QsciScintilla *editor);
    void setCreateDefault(bool createDefault);

    // tab管理..
    void addTab(const QString &text, const QString &filePath = "");
    void removeTab(int index);
    void setTabText(int index, const QString &text);
    void setTabToolTip(int index, const QString &toolTip);
    int count() const;
    void clear();

    // 文件操作..
    bool maybeSave(QsciScintilla *editor);
    bool saveFile(QsciScintilla *editor, const QString &filePath);
    void openFile(const QString &filePath);
    void openFileByArg(const QString &filePath);
    void setCurrentFile(QsciScintilla *editor, const QString &filePath);
    void createDefaultDoc();

    // 关闭所有标签页（用于应用程序退出）..
    bool closeAllEditors();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void currentEditorChanged(QsciScintilla *editor);
    void editorModificationChanged(QsciScintilla *editor, bool modified);
    void editorCreated(QsciScintilla *editor);
    void editorClosed(QsciScintilla *editor);
    void fileOpened(QsciScintilla *editor, const QString &filePath);
    void modificationChanged(bool modified);
    void copyAvailable(bool available);

public slots:
    void onNewFile();
    void onOpenFile(const QString &fileName = QString());
    bool onSave();
    bool onSaveAs();
    void tabCloseRequested(int index);
    void currentTabChanged(int index);
    void setCurrentTabIndex(int index);
    void documentWasModified();
    void onSwitchPopupClosed();
    void switchToEditorByIndex(int index);
    void showEditorSwitchPopup();

private slots:
    void on_pushButton_closeDoc_clicked();
    void on_comboBox_files_currentIndexChanged(int index);

private:
    void setEditorFont(QsciScintilla *editor);
    void connectEditorSignals(QsciScintilla *editor);

    Ui::MultiEditor *ui;
    EditorManager *m_editorManager;
    QsciScintilla *m_defaultEditor; // 指向默认空白编辑器的指针..
    EditorSwitchPopup *m_switchPopup;
};

#endif // MULTIEDITOR_H
