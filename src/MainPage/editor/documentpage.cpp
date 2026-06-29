#include "editor/documentpage.h"

#include "editor/customeditor.h"
#include "tool/markdown/markdownpreviewer.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <qlitehtmlwidget.h>

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
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_preview(nullptr)
    , m_previewer(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_syncingScroll(false)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_splitter);

    m_splitter->addWidget(editor);
    m_splitter->setChildrenCollapsible(false);

    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(180);

    connect(editor, &CustomEditor::copyAvailable, this, &DocumentPage::copyAvailableChanged);
    connect(editor, &CustomEditor::textChanged, this, &MarkdownDocumentPage::schedulePreviewUpdate);
    connect(m_updateTimer, &QTimer::timeout, this, &MarkdownDocumentPage::updatePreview);
    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MarkdownDocumentPage::syncPreviewScrollFromEditor);
}

bool MarkdownDocumentPage::isPreviewVisible() const
{
    return m_preview && m_preview->isVisible();
}

CustomEditor *MarkdownDocumentPage::takeEditor()
{
    if (m_editor)
    {
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
    m_preview->show();
    if (m_splitter->sizes().size() == 2)
    {
        const int width = qMax(1, m_splitter->width());
        m_splitter->setSizes({width * 2 / 3, width / 3});
    }
    updatePreview();
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

    if (m_preview)
    {
        m_preview->hide();
    }
    m_editor->setFocus();
    emit copyAvailableChanged(hasCopySelection());
}

void MarkdownDocumentPage::refreshPreview()
{
    if (isPreviewVisible())
    {
        schedulePreviewUpdate();
    }
}

void MarkdownDocumentPage::copyCurrentSelection()
{
    if (!isPreviewVisible())
    {
        DocumentPage::copyCurrentSelection();
        return;
    }

    QWidget *focus = QApplication::focusWidget();
    const bool editorFocused = m_editor && (focus == m_editor || m_editor->isAncestorOf(focus));
    const bool previewFocused = m_preview && (focus == m_preview || m_preview->isAncestorOf(focus));

    if (editorFocused && m_editor->hasSelectedText())
    {
        m_editor->copy();
        return;
    }

    if (previewFocused)
    {
        const QString selectedText = m_preview->selectedText();
        if (!selectedText.isEmpty())
        {
            QGuiApplication::clipboard()->setText(selectedText);
            return;
        }
    }

    if (m_editor && m_editor->hasSelectedText())
    {
        m_editor->copy();
        return;
    }

    const QString selectedText = m_preview ? m_preview->selectedText() : QString();
    if (!selectedText.isEmpty())
    {
        QGuiApplication::clipboard()->setText(selectedText);
    }
}

bool MarkdownDocumentPage::hasCopySelection() const
{
    if (isPreviewVisible())
    {
        return DocumentPage::hasCopySelection() || !m_preview->selectedText().isEmpty();
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
    m_preview = m_previewer->createPreviewWidget(QString(), QString(), this);
    connect(m_preview, &QLiteHtmlWidget::copyAvailable,
            this, &DocumentPage::copyAvailableChanged);
    connect(m_preview->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MarkdownDocumentPage::syncEditorScrollFromPreview);
    m_splitter->addWidget(m_preview);
    m_preview->hide();
}

void MarkdownDocumentPage::updatePreview()
{
    if (!m_previewer || !m_preview || !m_editor || !isPreviewVisible())
    {
        return;
    }

    const double ratio = previewScrollRatio();
    m_previewer->updatePreview(m_preview, m_editor->text(), m_editor->filePath());
    restorePreviewScroll(ratio);
}

void MarkdownDocumentPage::schedulePreviewUpdate()
{
    if (isPreviewVisible())
    {
        m_updateTimer->start();
    }
}

void MarkdownDocumentPage::syncPreviewScrollFromEditor()
{
    if (!isPreviewVisible() || m_syncingScroll)
    {
        return;
    }

    m_syncingScroll = true;
    setScrollRatio(m_preview, editorScrollRatio());
    m_syncingScroll = false;
}

void MarkdownDocumentPage::syncEditorScrollFromPreview()
{
    if (!m_editor || !isPreviewVisible() || m_syncingScroll)
    {
        return;
    }

    m_syncingScroll = true;
    setScrollRatio(m_editor, previewScrollRatio());
    m_syncingScroll = false;
}

void MarkdownDocumentPage::restorePreviewScroll(double ratio)
{
    QTimer::singleShot(0, this, [this, ratio]() {
        if (!isPreviewVisible() || !m_preview)
        {
            return;
        }

        m_syncingScroll = true;
        setScrollRatio(m_preview, ratio);
        m_syncingScroll = false;
    });
}

double MarkdownDocumentPage::editorScrollRatio() const
{
    if (!m_editor)
    {
        return 0.0;
    }

    const QScrollBar *bar = m_editor->verticalScrollBar();
    const int span = bar->maximum() - bar->minimum();
    return span > 0 ? double(bar->value() - bar->minimum()) / double(span) : 0.0;
}

double MarkdownDocumentPage::previewScrollRatio() const
{
    if (!m_preview)
    {
        return 0.0;
    }

    const QScrollBar *bar = m_preview->verticalScrollBar();
    const int span = bar->maximum() - bar->minimum();
    return span > 0 ? double(bar->value() - bar->minimum()) / double(span) : 0.0;
}

void MarkdownDocumentPage::setScrollRatio(QWidget *widget, double ratio)
{
    auto *area = qobject_cast<QAbstractScrollArea *>(widget);
    if (!area)
    {
        return;
    }

    QScrollBar *bar = area->verticalScrollBar();
    const int span = bar->maximum() - bar->minimum();
    bar->setValue(bar->minimum() + qRound(qBound(0.0, ratio, 1.0) * span));
}
