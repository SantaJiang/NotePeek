#include "editor/documentpage.h"

#include "editor/customeditor.h"
#include "foundation/performanceconstants.h"
#include "themecolors.h"
#include "tool/markdown/markdownpreviewer.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <qlitehtmlwidget.h>

#include <utility>

namespace {

// Thin vertical accent bar drawn over the preview viewport to mark the block
// matching the source cursor line.  It is a transparent, click-through child of
// the viewport positioned manually as the cursor moves and the preview scrolls.
class PreviewLineMarker final : public QWidget
{
public:
    explicit PreviewLineMarker(QWidget *parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        hide();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), ThemeColors::MarkdownPreview::linkFg());
    }
};

} // namespace

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

bool DocumentPage::isEditorHidden() const
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
    , m_scrollMapTimer(new QTimer(this))
    , m_previewLineMarker(nullptr)
    , m_currentSourceLine(1)
    , m_pendingScrollMapSourceLine(1)
    , m_syncingScroll(false)
    , m_latestPreviewRequestId(0)
    , m_mode(SourceOnly)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_splitter);

    m_splitter->addWidget(editor);
    m_splitter->setChildrenCollapsible(false);

    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(PerformanceConstants::MarkdownPreviewDelayMs);
    m_scrollMapTimer->setSingleShot(true);
    m_scrollMapTimer->setInterval(40);

    connect(editor, &CustomEditor::copyAvailable, this, &DocumentPage::copyAvailableChanged);
    connect(editor, &CustomEditor::textChanged, this, &MarkdownDocumentPage::schedulePreviewUpdate);
    connect(editor, &CustomEditor::cursorPositionChanged,
            this, &MarkdownDocumentPage::onEditorCursorMoved);
    connect(m_updateTimer, &QTimer::timeout, this, &MarkdownDocumentPage::updatePreview);
    connect(m_scrollMapTimer, &QTimer::timeout, this, [this]() {
        if (!isPreviewVisible() || !m_preview)
        {
            return;
        }

        rebuildPreviewScrollMap();
        const int y = previewYForSourceLine(m_pendingScrollMapSourceLine);
        if (y >= 0 && !m_syncingScroll)
        {
            m_syncingScroll = true;
            QScrollBar *bar = m_preview->verticalScrollBar();
            const int centeredY = y - m_preview->viewport()->height() / 2;
            bar->setValue(qBound(bar->minimum(), centeredY, bar->maximum()));
            m_syncingScroll = false;
        }
        updatePreviewLineMarker();
    });
    connect(editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MarkdownDocumentPage::syncPreviewScrollFromEditor);
    connect(m_splitter, &QSplitter::splitterMoved,
            this, &MarkdownDocumentPage::schedulePreviewScrollMapRebuild);
}

bool MarkdownDocumentPage::isPreviewVisible() const
{
    return m_mode == Split;
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
    // Flip the side preview on or off beside the source editor.
    setViewMode(m_mode == Split ? SourceOnly : Split, previewer);
    return isPreviewVisible();
}

void MarkdownDocumentPage::hidePreview()
{
    setViewMode(SourceOnly, nullptr);
}

void MarkdownDocumentPage::setInitialViewMode(bool preview, MarkdownPreviewer *previewer, bool giveFocus)
{
    setViewMode(preview ? Split : SourceOnly, previewer, giveFocus);
}

void MarkdownDocumentPage::setViewMode(ViewMode mode, MarkdownPreviewer *previewer, bool giveFocus)
{
    if (mode != SourceOnly)
    {
        ensurePreview(previewer);
        // Without a preview widget (no previewer ever supplied) fall back to
        // the source view instead of showing an empty pane.
        if (!m_preview)
        {
            mode = SourceOnly;
        }
    }

    m_mode = mode;

    switch (mode)
    {
    case SourceOnly:
        if (m_editor)
        {
            m_editor->show();
        }
        if (m_preview)
        {
            m_preview->hide();
        }
        if (m_previewLineMarker)
        {
            m_previewLineMarker->hide();
        }
        break;
    case Split:
        if (m_editor)
        {
            m_editor->show();
        }
        m_preview->show();
        if (m_splitter->sizes().size() == 2)
        {
            const int width = qMax(1, m_splitter->width());
            m_splitter->setSizes({width / 2, width / 2});
        }
        break;
    }

    if (mode == Split)
    {
        updatePreview();
    }

    // The source editor is always visible now, so keep keyboard focus there.
    if (m_editor && giveFocus)
    {
        m_editor->setFocus();
    }

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

bool MarkdownDocumentPage::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_preview && event->type() == QEvent::Resize)
    {
        schedulePreviewScrollMapRebuild();
    }

    return DocumentPage::eventFilter(watched, event);
}

void MarkdownDocumentPage::ensurePreview(MarkdownPreviewer *previewer)
{
    // Remember the previewer, but never clobber a good one with a null passed
    // by an internal transition (e.g. hidePreview()).
    if (previewer)
    {
        m_previewer = previewer;
    }

    if (m_preview || !m_previewer)
    {
        return;
    }

    m_preview = m_previewer->createPreviewWidget(QString(), QString(), this);
    connect(m_preview, &QLiteHtmlWidget::copyAvailable,
            this, &DocumentPage::copyAvailableChanged);
    connect(m_preview->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MarkdownDocumentPage::syncEditorScrollFromPreview);
    connect(m_preview->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &MarkdownDocumentPage::updatePreviewLineMarker);
    connect(m_previewer, &MarkdownPreviewer::previewRendered,
            this, &MarkdownDocumentPage::onPreviewRendered,
            Qt::UniqueConnection);
    m_preview->installEventFilter(this);
    m_splitter->addWidget(m_preview);
    m_preview->hide();

    m_previewLineMarker = new PreviewLineMarker(m_preview->viewport());
}

void MarkdownDocumentPage::updatePreview()
{
    if (!m_previewer || !m_preview || !m_editor || !isPreviewVisible())
    {
        return;
    }

    const int documentLength = int(m_editor->SendScintilla(QsciScintillaBase::SCI_GETLENGTH));
    if (documentLength > PerformanceConstants::MarkdownLargePreviewLimit)
    {
        m_latestPreviewRequestId = 0;
        m_previewSourceLines.clear();
        m_previewLineMap.clear();
        m_previewer->updatePreview(m_preview,
                                   tr("Preview disabled for large Markdown files."),
                                   m_editor->filePath());
        updatePreviewLineMarker();
        return;
    }

    // Snapshot straight from Scintilla's buffer: text() would copy the whole
    // document twice (SCI_GETTEXT + UTF-16 conversion); this converts once.
    const char *buffer = reinterpret_cast<const char *>(
        m_editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETCHARACTERPOINTER));
    const QString markdown = buffer
                                 ? QString::fromUtf8(buffer, documentLength)
                                 : m_editor->text();
    m_latestPreviewRequestId = m_previewer->renderPreviewAsync(markdown,
                                                               m_editor->filePath(),
                                                               this);
}

void MarkdownDocumentPage::onPreviewRendered(const MarkdownRenderResult &result)
{
    // The previewer is shared across pages/panes and broadcasts to all of them,
    // so guard on both the request id and the file path before adopting a
    // result into this page's preview.
    if (result.requestId != m_latestPreviewRequestId || !m_previewer || !m_preview || !m_editor || !isPreviewVisible())
    {
        return;
    }
    if (result.filePath != m_editor->filePath())
    {
        return;
    }

    const int sourceLine = sourceLineAtEditorCenter();
    const double ratio = previewScrollRatio();
    const int firstVisibleLine = int(m_editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE));
    const int xOffset = int(m_editor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET));

    m_syncingScroll = true;
    m_previewer->applyRenderedPreview(m_preview, result);
    m_previewSourceLines = result.sourceLines;
    rebuildPreviewScrollMap();
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);

    QTimer::singleShot(0, this, [this, sourceLine, ratio, firstVisibleLine, xOffset]() {
        if (!isPreviewVisible() || !m_preview || !m_editor)
        {
            m_syncingScroll = false;
            updatePreviewLineMarker();
            return;
        }

        m_editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
        m_editor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);

        QScrollBar *editorBar = m_editor->verticalScrollBar();
        QScrollBar *previewBar = m_preview->verticalScrollBar();
        if (editorBar->value() <= editorBar->minimum())
        {
            previewBar->setValue(previewBar->minimum());
            m_syncingScroll = false;
            return;
        }
        if (editorBar->value() >= editorBar->maximum())
        {
            previewBar->setValue(previewBar->maximum());
            m_syncingScroll = false;
            return;
        }

        const int y = previewYForSourceLine(sourceLine);
        if (y >= 0)
        {
            const int centeredY = y - m_preview->viewport()->height() / 2;
            previewBar->setValue(qBound(previewBar->minimum(), centeredY, previewBar->maximum()));
        }
        else
        {
            setScrollRatio(m_preview, ratio);
        }
        m_syncingScroll = false;
        updatePreviewLineMarker();
    });
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
    if (!m_editor || !isPreviewVisible() || m_syncingScroll)
    {
        return;
    }

    m_syncingScroll = true;
    QScrollBar *editorBar = m_editor->verticalScrollBar();
    QScrollBar *previewBar = m_preview->verticalScrollBar();
    if (editorBar->value() <= editorBar->minimum())
    {
        previewBar->setValue(previewBar->minimum());
        m_syncingScroll = false;
        return;
    }
    if (editorBar->value() >= editorBar->maximum())
    {
        previewBar->setValue(previewBar->maximum());
        m_syncingScroll = false;
        return;
    }

    const int y = previewYForSourceLine(sourceLineAtEditorCenter());
    if (y >= 0)
    {
        const int centeredY = y - m_preview->viewport()->height() / 2;
        previewBar->setValue(qBound(previewBar->minimum(), centeredY, previewBar->maximum()));
    }
    else
    {
        setScrollRatio(m_preview, editorScrollRatio());
    }
    m_syncingScroll = false;
}

void MarkdownDocumentPage::syncEditorScrollFromPreview()
{
    if (!m_editor || !isPreviewVisible() || m_syncingScroll)
    {
        return;
    }

    m_syncingScroll = true;
    QScrollBar *editorBar = m_editor->verticalScrollBar();
    QScrollBar *previewBar = m_preview->verticalScrollBar();
    if (previewBar->value() <= previewBar->minimum())
    {
        editorBar->setValue(editorBar->minimum());
        m_syncingScroll = false;
        return;
    }
    if (previewBar->value() >= previewBar->maximum())
    {
        editorBar->setValue(editorBar->maximum());
        m_syncingScroll = false;
        return;
    }

    const int line = sourceLineForPreviewY(m_preview->verticalScrollBar()->value()
                                           + m_preview->viewport()->height() / 2);
    if (line >= 1)
    {
        scrollEditorToSourceLineCentered(line);
    }
    else
    {
        setScrollRatio(m_editor, previewScrollRatio());
    }
    m_syncingScroll = false;
}

void MarkdownDocumentPage::rebuildPreviewScrollMap()
{
    m_previewLineMap.clear();
    if (!m_preview)
    {
        return;
    }

    m_previewLineMap.reserve(m_previewSourceLines.size());
    for (int line : std::as_const(m_previewSourceLines))
    {
        const int y = m_preview->anchorY(QStringLiteral("np-src-line-%1").arg(line));
        if (y >= 0)
        {
            m_previewLineMap.append(qMakePair(line, y));
        }
    }

    std::sort(m_previewLineMap.begin(), m_previewLineMap.end(),
              [](const QPair<int, int> &a, const QPair<int, int> &b) {
                  return a.first < b.first;
              });
}

void MarkdownDocumentPage::schedulePreviewScrollMapRebuild()
{
    if (!isPreviewVisible())
    {
        return;
    }

    m_pendingScrollMapSourceLine = sourceLineAtEditorCenter();
    m_scrollMapTimer->start();
}

void MarkdownDocumentPage::onEditorCursorMoved(int line, int /*index*/)
{
    m_currentSourceLine = line + 1;
    updatePreviewLineMarker();
}

void MarkdownDocumentPage::updatePreviewLineMarker()
{
    if (!m_previewLineMarker)
    {
        return;
    }

    if (!m_preview || !isPreviewVisible())
    {
        m_previewLineMarker->hide();
        return;
    }

    const int contentY = previewYForSourceLine(m_currentSourceLine);
    if (contentY < 0)
    {
        m_previewLineMarker->hide();
        return;
    }

    QWidget *viewport = m_preview->viewport();
    const int viewportY = contentY - m_preview->verticalScrollBar()->value();

    // One preview text line is a good approximation of the marked block height.
    const int markerHeight = qMax(18, m_preview->fontMetrics().height() + 6);
    const int markerWidth = 3;
    // Keep a small visual gap between the marker and the rendered Markdown.
    constexpr int markerContentGap = 5;
    const int markerX = qMax(0, MarkdownPreviewLayout::ScreenHorizontalMargin
                                   - markerWidth - markerContentGap);

    if (viewportY + markerHeight < 0 || viewportY > viewport->height())
    {
        m_previewLineMarker->hide();
        return;
    }

    m_previewLineMarker->setGeometry(markerX, viewportY, markerWidth, markerHeight);
    m_previewLineMarker->show();
    m_previewLineMarker->raise();
}

int MarkdownDocumentPage::sourceLineAtEditorCenter() const
{
    if (!m_editor)
    {
        return 1;
    }

    const int firstVisibleLine = int(m_editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE));
    const int visibleLines = qMax(1, int(m_editor->SendScintilla(QsciScintillaBase::SCI_LINESONSCREEN)));
    const int centerVisibleLine = firstVisibleLine + visibleLines / 2;
    const int documentLine = int(m_editor->SendScintilla(QsciScintillaBase::SCI_DOCLINEFROMVISIBLE,
                                                         centerVisibleLine));
    return qMax(0, documentLine) + 1;
}

void MarkdownDocumentPage::scrollEditorToSourceLineCentered(int line)
{
    if (!m_editor)
    {
        return;
    }

    const int visibleLines = qMax(1, int(m_editor->SendScintilla(QsciScintillaBase::SCI_LINESONSCREEN)));
    const int visibleLine = int(m_editor->SendScintilla(QsciScintillaBase::SCI_VISIBLEFROMDOCLINE,
                                                        qMax(0, line - 1)));
    m_editor->setFirstVisibleLine(qMax(0, visibleLine - visibleLines / 2));
}

int MarkdownDocumentPage::previewYForSourceLine(int line) const
{
    if (m_previewLineMap.isEmpty())
    {
        return -1;
    }

    const auto nextIt = std::lower_bound(
        m_previewLineMap.cbegin(), m_previewLineMap.cend(), line,
        [](const QPair<int, int> &entry, int sourceLine) { return entry.first < sourceLine; });
    if (nextIt == m_previewLineMap.cbegin())
        return nextIt->second;
    if (nextIt == m_previewLineMap.cend())
        return m_previewLineMap.last().second;

    const auto previous = *(nextIt - 1);
    const auto next = *nextIt;
    const int lineSpan = next.first - previous.first;
    if (lineSpan <= 0)
        return previous.second;
    const double t = double(line - previous.first) / double(lineSpan);
    return previous.second + qRound(t * double(next.second - previous.second));
}

int MarkdownDocumentPage::sourceLineForPreviewY(int y) const
{
    if (m_previewLineMap.isEmpty())
    {
        return -1;
    }

    const auto nextIt = std::lower_bound(
        m_previewLineMap.cbegin(), m_previewLineMap.cend(), y,
        [](const QPair<int, int> &entry, int previewY) { return entry.second < previewY; });
    if (nextIt == m_previewLineMap.cbegin())
        return nextIt->first;
    if (nextIt == m_previewLineMap.cend())
        return m_previewLineMap.last().first;

    const auto previous = *(nextIt - 1);
    const auto next = *nextIt;
    const int ySpan = next.second - previous.second;
    if (ySpan <= 0)
        return previous.first;
    const double t = double(y - previous.second) / double(ySpan);
    return previous.first + qRound(t * double(next.first - previous.first));
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
