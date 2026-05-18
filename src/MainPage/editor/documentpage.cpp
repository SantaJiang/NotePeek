#include "documentpage.h"

#include "customeditor.h"
#include "../tool/markdown/markdownpreviewer.h"

#include <QStackedWidget>
#include <QTextBrowser>
#include <QTextCursor>
#include <QVBoxLayout>

DocumentPage::DocumentPage(CustomEditor *editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
{
}

CustomEditor *DocumentPage::editor() const
{
    return m_editor;
}

CustomEditor *DocumentPage::takeEditor()
{
    CustomEditor *editor = m_editor;
    if (editor)
    {
        editor->setParent(nullptr);
    }
    m_editor = nullptr;
    return editor;
}

bool DocumentPage::isPreviewVisible() const
{
    return false;
}

bool DocumentPage::togglePreview(MarkdownPreviewer *previewer)
{
    Q_UNUSED(previewer);
    return false;
}

void DocumentPage::hidePreview()
{
}

void DocumentPage::refreshPreview()
{
}

void DocumentPage::copyCurrentSelection()
{
    if (m_editor)
    {
        m_editor->copy();
    }
}

bool DocumentPage::hasCopySelection() const
{
    return m_editor && m_editor->hasSelectedText();
}

PlainDocumentPage::PlainDocumentPage(CustomEditor *editor, QWidget *parent)
    : DocumentPage(editor, parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(editor);
}

MarkdownDocumentPage::MarkdownDocumentPage(CustomEditor *editor, QWidget *parent)
    : DocumentPage(editor, parent)
    , m_stack(new QStackedWidget(this))
    , m_preview(nullptr)
    , m_previewer(nullptr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_stack);

    m_stack->addWidget(editor);
    m_stack->setCurrentWidget(editor);

    connect(editor, &CustomEditor::copyAvailable, this, &DocumentPage::copyAvailableChanged);
}

bool MarkdownDocumentPage::isPreviewVisible() const
{
    return m_preview && m_stack->currentWidget() == m_preview;
}

CustomEditor *MarkdownDocumentPage::takeEditor()
{
    if (m_editor)
    {
        m_stack->removeWidget(m_editor);
        m_editor->setParent(nullptr);
    }
    CustomEditor *editor = m_editor;
    m_editor = nullptr;
    return editor;
}

bool MarkdownDocumentPage::togglePreview(MarkdownPreviewer *previewer)
{
    if (isPreviewVisible())
    {
        hidePreview();
        return false;
    }

    ensurePreview(previewer);
    updatePreview();
    m_stack->setCurrentWidget(m_preview);
    m_preview->setFocus();
    emit copyAvailableChanged(hasCopySelection());
    return true;
}

void MarkdownDocumentPage::hidePreview()
{
    if (!m_editor)
    {
        return;
    }

    m_stack->setCurrentWidget(m_editor);
    m_editor->setFocus();
    emit copyAvailableChanged(hasCopySelection());
}

void MarkdownDocumentPage::refreshPreview()
{
    if (isPreviewVisible())
    {
        updatePreview();
    }
}

void MarkdownDocumentPage::copyCurrentSelection()
{
    if (isPreviewVisible())
    {
        m_preview->copy();
        return;
    }

    DocumentPage::copyCurrentSelection();
}

bool MarkdownDocumentPage::hasCopySelection() const
{
    if (isPreviewVisible())
    {
        return !m_preview->textCursor().selectedText().isEmpty();
    }

    return DocumentPage::hasCopySelection();
}

void MarkdownDocumentPage::ensurePreview(MarkdownPreviewer *previewer)
{
    if (m_preview)
    {
        m_previewer = previewer;
        return;
    }

    m_previewer = previewer;
    m_preview = new QTextBrowser(this);
    m_preview->setReadOnly(true);
    m_preview->setOpenExternalLinks(true);
    connect(m_preview, &QTextBrowser::copyAvailable,
            this, &DocumentPage::copyAvailableChanged);
    m_stack->addWidget(m_preview);
}

void MarkdownDocumentPage::updatePreview()
{
    if (!m_previewer || !m_preview || !m_editor)
    {
        return;
    }

    m_previewer->updatePreview(m_preview, m_editor->text(), m_editor->filePath());
}
