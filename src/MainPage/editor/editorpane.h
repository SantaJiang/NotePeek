#ifndef EDITORPANE_H
#define EDITORPANE_H

#include <QWidget>

class CustomEditor;
class DocumentPage;
class EditorSwitchPopup;
class MarkdownPreviewer;

namespace Ui
{
class EditorPane;
}

// Pure view of one editor pane: a tab stack of document pages plus the
// top bar (file combo box, close and split-view buttons) and the
// Ctrl+Tab switch popup. All document and file logic lives in
// DocumentController.
class EditorPane : public QWidget
{
    Q_OBJECT

public:
    explicit EditorPane(QWidget *parent = nullptr);
    ~EditorPane();

    // Page container
    int addPage(DocumentPage *page, const QString &title, const QString &toolTip);
    void removePage(int index);
    void replacePage(int index, DocumentPage *page);
    int count() const;
    int currentIndex() const;
    void setCurrentIndex(int index);

    // Queries
    DocumentPage *currentPage() const;
    DocumentPage *pageAt(int index) const;
    DocumentPage *pageForEditor(CustomEditor *editor) const;
    CustomEditor *currentEditor() const;
    CustomEditor *editorAt(int index) const;
    int indexOfEditor(CustomEditor *editor) const;
    QString filePathAt(int index) const;
    QString pageTitle(int index) const;
    void setPageTitle(int index, const QString &title);
    void setPageToolTip(int index, const QString &toolTip);
    void focusEditor(CustomEditor *editor);

    // Markdown preview (delegates to the current page)
    bool isCurrentEditorPreviewVisible() const;
    bool toggleMarkdownPreview(MarkdownPreviewer *previewer);
    void hideMarkdownPreview(CustomEditor *editor);
    void refreshVisibleMarkdownPreview();
    void copyCurrentSelection();

    // Chrome
    void setSplitViewButtonChecked(bool checked);
    void setSplitViewButtonVisible(bool visible);
    void setTopbarVisible(bool visible);

signals:
    void pageCloseRequested(int index);
    void currentPageChanged(int index);
    void splitViewToggled(bool checked);

private slots:
    void on_pushButton_closeDoc_clicked();
    void on_pushButton_splitView_clicked(bool checked);
    void on_comboBox_files_currentIndexChanged(int index);
    void onTabCurrentChanged(int index);
    void showEditorSwitchPopup();

private:
    DocumentPage *pageForWidget(QWidget *widget) const;
    void syncComboBoxIndex(int index);

    Ui::EditorPane *ui;
    EditorSwitchPopup *m_switchPopup;
};

#endif // EDITORPANE_H
