#ifndef DOCUMENTPAGE_H
#define DOCUMENTPAGE_H

#include <QWidget>
#include <QVector>

#include "tool/markdown/markdownpreviewer.h"

class CustomEditor;
class MarkdownPreviewer;
class QLiteHtmlWidget;
class QSplitter;
class QTimer;
class QWidget;

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
    // Emitted after the user changes the Markdown preview layout (toolbar
    // toggle or splitter collapse).  Programmatic initial layout does not
    // emit this, so opening a file does not overwrite the remembered choice.
    void previewLayoutChanged();

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
    // Split shows the editor with the rendered preview beside it, and
    // PreviewOnly hides the source so only the preview remains.
    enum ViewMode
    {
        SourceOnly,
        Split,
        PreviewOnly
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

    ViewMode viewMode() const { return m_mode; }

    // Choose the layout a freshly opened Markdown file starts in.  giveFocus
    // is false for background opens so the file being activated elsewhere
    // keeps keyboard focus.  Does not record the layout as the user's last
    // choice; that happens only on toggle or splitter collapse.
    void setInitialViewMode(ViewMode mode, MarkdownPreviewer *previewer, bool giveFocus = true);

    // Hide the source pane when the user has dragged it narrower than the
    // collapse threshold.  Exposed so tests can drive the same decision the
    // splitter uses without synthesizing a mouse drag.
    void considerCollapsingSource(int sourceWidth, int totalWidth);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setViewMode(ViewMode mode, MarkdownPreviewer *previewer, bool giveFocus, bool remember);
    void ensurePreview(MarkdownPreviewer *previewer);
    void updatePreview();
    void schedulePreviewUpdate();
    void syncPreviewScrollFromEditor();
    void syncEditorScrollFromPreview();
    void onPreviewRendered(const MarkdownRenderResult &result);
    void rebuildPreviewScrollMap();
    void schedulePreviewScrollMapRebuild();
    void onSplitterMoved(int pos, int index);
    void onEditorCursorMoved(int line, int index);
    void updatePreviewLineMarker();
    int sourceLineAtEditorCenter() const;
    void scrollEditorToSourceLineCentered(int line);
    int previewYForSourceLine(int line) const;
    int sourceLineForPreviewY(int y) const;

    // One laid-out source range. Cells that share a range are merged, because
    // litehtml gives <tr> an empty box and puts the row rectangle on the cells.
    struct PreviewSourceBox
    {
        int startLine = 0;
        int endLine = 0;
        int top = 0;
        int bottom = 0;

        int lineSpan() const { return endLine - startLine; }
        int height() const { return bottom - top; }
    };

    struct PreviewSourceMarker
    {
        int top = -1;
        int height = 0;
        int scrollY = -1;

        bool valid() const { return top >= 0 && height > 0 && scrollY >= 0; }
    };

    PreviewSourceMarker previewMarkerForLine(int line) const;
    double editorScrollRatio() const;
    double previewScrollRatio() const;
    static void setScrollRatio(QWidget *widget, double ratio);

    QSplitter *m_splitter;
    QLiteHtmlWidget *m_preview;
    MarkdownPreviewer *m_previewer;
    QTimer *m_updateTimer;
    QTimer *m_scrollMapTimer;
    QVector<PreviewSourceBox> m_previewSourceBoxes;
    // Accent bar over the preview viewport. Its height is the laid-out block
    // that owns the source cursor line, not a single preview text line.
    QWidget *m_previewLineMarker;
    int m_currentSourceLine;
    int m_pendingScrollMapSourceLine;
    bool m_syncingScroll;
    bool m_applyingViewMode;
    quint64 m_latestPreviewRequestId;
    ViewMode m_mode;
};

#endif // DOCUMENTPAGE_H
