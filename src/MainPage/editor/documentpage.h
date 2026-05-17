#ifndef DOCUMENTPAGE_H
#define DOCUMENTPAGE_H

#include <QWidget>

class CustomEditor;
class MarkdownPreviewer;
class QTextBrowser;
class QStackedWidget;

class DocumentPage : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentPage(CustomEditor *editor, QWidget *parent = nullptr);

    CustomEditor *editor() const;
    virtual CustomEditor *takeEditor();

    virtual bool isPreviewVisible() const;
    virtual bool togglePreview(MarkdownPreviewer *previewer);
    virtual void hidePreview();
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
    explicit MarkdownDocumentPage(CustomEditor *editor, QWidget *parent = nullptr);

    bool isPreviewVisible() const override;
    CustomEditor *takeEditor() override;
    bool togglePreview(MarkdownPreviewer *previewer) override;
    void hidePreview() override;
    void copyCurrentSelection() override;
    bool hasCopySelection() const override;

private:
    void ensurePreview(MarkdownPreviewer *previewer);
    void updatePreview();

    QStackedWidget *m_stack;
    QTextBrowser *m_preview;
    MarkdownPreviewer *m_previewer;
};

#endif // DOCUMENTPAGE_H
