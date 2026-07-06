#ifndef DOCUMENTPAGE_H
#define DOCUMENTPAGE_H

#include <QWidget>
#include <QPair>
#include <QVector>

class CustomEditor;
class MarkdownPreviewer;
class QLiteHtmlWidget;
class QSplitter;
class QTimer;

class DocumentPage : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentPage(CustomEditor *editor, QWidget *parent = nullptr);

    CustomEditor *editor() const;
    virtual CustomEditor *takeEditor();

    virtual bool isPreviewVisible() const;
    // True when the source editor is not visible/editable (Markdown preview-only
    // mode).  The main window uses this to disable editing actions that would
    // otherwise operate on the hidden editor.
    virtual bool isEditorHidden() const;
    virtual bool togglePreview(MarkdownPreviewer *previewer);
    virtual void hidePreview();
    virtual void refreshPreview();
    virtual void copyCurrentSelection();
    virtual bool hasCopySelection() const;

signals:
    void copyAvailableChanged(bool available);

protected:
    CustomEditor *m_editor;
};

class PlainDocumentPage : public DocumentPage
{
    Q_OBJECT

public:
    explicit PlainDocumentPage(CustomEditor *editor, QWidget *parent = nullptr);
};

class MarkdownDocumentPage : public DocumentPage
{
    Q_OBJECT

public:
    // How a Markdown page is laid out.  SourceOnly shows just the editor,
    // PreviewOnly shows just the rendered preview, and Split shows both.
    enum ViewMode
    {
        SourceOnly,
        PreviewOnly,
        Split
    };

    explicit MarkdownDocumentPage(CustomEditor *editor, QWidget *parent = nullptr);

    bool isPreviewVisible() const override;
    bool isEditorHidden() const override;
    CustomEditor *takeEditor() override;
    bool togglePreview(MarkdownPreviewer *previewer) override;
    void hidePreview() override;
    void refreshPreview() override;
    void copyCurrentSelection() override;
    bool hasCopySelection() const override;

    // Choose the mode a freshly opened Markdown file starts in (source or
    // preview).  The chosen single-pane mode becomes the "home" the split
    // toggle returns to.  giveFocus is false for background opens so the file
    // being activated elsewhere keeps keyboard focus.
    void setInitialViewMode(bool preview, MarkdownPreviewer *previewer, bool giveFocus = true);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setViewMode(ViewMode mode, MarkdownPreviewer *previewer, bool giveFocus = true);
    void ensurePreview(MarkdownPreviewer *previewer);
    void updatePreview();
    void schedulePreviewUpdate();
    void syncPreviewScrollFromEditor();
    void syncEditorScrollFromPreview();
    void rebuildPreviewScrollMap();
    void schedulePreviewScrollMapRebuild();
    int sourceLineAtEditorCenter() const;
    void scrollEditorToSourceLineCentered(int line);
    int previewYForSourceLine(int line) const;
    int sourceLineForPreviewY(int y) const;
    double editorScrollRatio() const;
    double previewScrollRatio() const;
    static void setScrollRatio(QWidget *widget, double ratio);

    QSplitter *m_splitter;
    QLiteHtmlWidget *m_preview;
    MarkdownPreviewer *m_previewer;
    QTimer *m_updateTimer;
    QVector<QPair<int, int>> m_previewLineMap;
    bool m_syncingScroll;
    ViewMode m_mode;
    ViewMode m_homeMode;
};

#endif // DOCUMENTPAGE_H
