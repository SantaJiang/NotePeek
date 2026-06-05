#ifndef MULTIEDITOR_H
#define MULTIEDITOR_H

#include <QWidget>
#include <QUrl>
#include "editormanager.h"

class DocumentPage;
class EditorSwitchPopup;
class FindReplaceManager;
class MarkdownPreviewer;

namespace Ui
{
class MultiEditor;
}

class CustomEditor;
class MultiEditor : public QWidget
{
    Q_OBJECT
public:
    explicit MultiEditor(QWidget *parent = nullptr, bool createDefault = true);
    ~MultiEditor();

    // 编辑器管理..
    CustomEditor *currentEditor() const;
    bool isCurrentEditorPreviewVisible() const;
    int findEditorIndex(CustomEditor *editor) const;
    int findFileIndex(const QString &filePath) const;
    int currentIndex() const;
    QString filePathAt(int index) const;
    void switchToEditor(CustomEditor *editor);
    void setCreateDefault(bool createDefault);
    void setFindReplaceManagerForCurrentEditor(FindReplaceManager *manager);
    bool toggleMarkdownPreview(MarkdownPreviewer *previewer);
    void hideMarkdownPreview(CustomEditor *editor);
    void refreshVisibleMarkdownPreview();
    void copyCurrentSelection();
    void setSplitViewButtonChecked(bool checked);
    EditorManager *editorManager() const { return m_editorManager; }

    // tab管理..
    void addTab(const QString &text, const QString &filePath = "");
    void removeTab(int index);
    void setTabText(int index, const QString &text);
    void setTabToolTip(int index, const QString &toolTip);
    int count() const;
    void clear();

    // 文件操作..
    bool maybeSave(CustomEditor *editor);
    bool saveFile(CustomEditor *editor, const QString &filePath);
    void openFile(const QString &filePath, bool activate = true);
    void setCurrentFile(CustomEditor *editor, const QString &filePath);
    void createDefaultDoc();

    // 关闭所有标签页（用于应用程序退出）..
    bool closeAllEditors();

signals:
    void currentEditorChanged(CustomEditor *editor);
    void editorModificationChanged(CustomEditor *editor, bool modified);
    void editorCreated(CustomEditor *editor);
    void editorClosed(CustomEditor *editor);
    void fileOpened(CustomEditor *editor, const QString &filePath);
    void currentFileChanged(CustomEditor *editor, const QString &filePath);
    void modificationChanged(bool modified);
    void copyAvailable(bool available);
    void filesDropped(const QList<QUrl> &urls);
    void splitViewToggled(bool checked);

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
    void on_pushButton_splitView_clicked(bool checked);
    void on_comboBox_files_currentIndexChanged(int index);

private:
    void connectEditorSignals(CustomEditor *editor);
    QString promptSaveFileName(QWidget *parent = nullptr);
    bool closeDefaultEditorIfUnused();
    DocumentPage *createDocumentPage(CustomEditor *editor, const QString &filePath);
    void refreshDocumentPageForFile(CustomEditor *editor, const QString &filePath);
    DocumentPage *currentPage() const;
    DocumentPage *pageForWidget(QWidget *widget) const;
    DocumentPage *pageForEditor(CustomEditor *editor) const;

    Ui::MultiEditor *ui;
    EditorManager *m_editorManager;
    CustomEditor *m_defaultEditor; // 指向默认空白编辑器的指针..
    EditorSwitchPopup *m_switchPopup;
};

#endif // MULTIEDITOR_H
