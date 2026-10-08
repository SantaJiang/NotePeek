#include "editor/documentpage.h"

#include "editor/customeditor.h"
#include "foundation/performanceconstants.h"
#include "themecolors.h"
#include "tool/markdown/markdownpreviewer.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QHash>
#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <qlitehtmlwidget.h>

#include <utility>

namespace {

// Vertical accent drawn over the preview viewport for the source range under
// the cursor. It is a transparent, click-through child of the viewport and is
// repositioned as the cursor moves and the preview scrolls.
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
    , m_applyingViewMode(false)
    , m_latestPreviewRequestId(0)
    , m_mode(SourceOnly)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_splitter);

    editor->setMinimumWidth(0);
    m_splitter->addWidget(editor);
    setFocusProxy(editor);
    // Keep the preview from collapsing; the source pane is allowed to shrink
    // so the user can drag it past the threshold that snaps to PreviewOnly.
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setCollapsible(0, true);

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
            this, &MarkdownDocumentPage::onSplitterMoved);
}

bool MarkdownDocumentPage::isPreviewVisible() const
{
    return m_mode != SourceOnly;
}

bool MarkdownDocumentPage::isEditorHidden() const
{
    return m_mode == PreviewOnly;
}

CustomEditor *MarkdownDocumentPage::takeEditor()
{
    if (focusProxy() == m_editor)
        setFocusProxy(nullptr);
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
    // Preview-only collapses back to source, and turning preview on from
    // source always restores the side-by-side split rather than preview-only.
    const ViewMode next = (m_mode == SourceOnly) ? Split : SourceOnly;
    setViewMode(next, previewer, true, true);
    return isPreviewVisible();
}

void MarkdownDocumentPage::hidePreview()
{
    // Programmatic resets (file-type refresh) must not steal keyboard focus
    // from whatever the user is currently editing.
    setViewMode(SourceOnly, nullptr, false, false);
}

void MarkdownDocumentPage::setInitialViewMode(ViewMode mode, MarkdownPreviewer *previewer, bool giveFocus)
{
    setViewMode(mode, previewer, giveFocus, false);
}

void MarkdownDocumentPage::considerCollapsingSource(int sourceWidth, int totalWidth)
{
    if (m_applyingViewMode || m_mode != Split)
        return;

    // Ignore tiny panes (for example a page that has not been laid out yet)
    // so an initial 50/50 split cannot snap to preview-only.
    constexpr int kSourceCollapseWidth = 96;
    constexpr int kMinTotalWidthForSourceCollapse = 280;
    if (totalWidth < kMinTotalWidthForSourceCollapse || sourceWidth > kSourceCollapseWidth)
        return;

    setViewMode(PreviewOnly, m_previewer, true, true);
}

void MarkdownDocumentPage::onSplitterMoved(int /*pos*/, int /*index*/)
{
    schedulePreviewScrollMapRebuild();
    const QList<int> sizes = m_splitter->sizes();
    if (sizes.size() == 2)
        considerCollapsingSource(sizes.at(0), sizes.at(0) + sizes.at(1));
}

void MarkdownDocumentPage::setViewMode(ViewMode mode, MarkdownPreviewer *previewer, bool giveFocus, bool remember)
{
    QScopedValueRollback applying(m_applyingViewMode, true);

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

    if (mode == m_mode)
        return;

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
        if (m_editor)
        {
            setFocusProxy(m_editor);
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
        if (m_editor)
        {
            setFocusProxy(m_editor);
        }
        break;
    case PreviewOnly:
        m_preview->show();
        setFocusProxy(m_preview);
        // Focus the preview before hiding the editor so Qt does not pick a
        // random widget when the focused source disappears.
        if (giveFocus || (m_editor && m_editor->hasFocus()))
        {
            m_preview->setFocus();
        }
        if (m_editor)
        {
            m_editor->hide();
        }
        break;
    }

    if (mode != SourceOnly)
    {
        updatePreview();
    }

    if (mode != PreviewOnly && m_editor && giveFocus)
    {
        m_editor->setFocus();
    }

    emit copyAvailableChanged(hasCopySelection());
    if (remember)
    {
        emit previewLayoutChanged();
    }
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
    m_splitter->setCollapsible(0, true);
    m_splitter->setCollapsible(1, false);
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
        m_previewSourceBoxes.clear();
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
    m_previewSourceBoxes.clear();
    if (!m_preview)
        return;

    QHash<quint64, int> indexByRange;
    const QVector<QLiteHtmlSourceBox> rawBoxes = m_preview->sourcePositionBoxes();
    for (const QLiteHtmlSourceBox &rawBox : rawBoxes) {
        if (rawBox.startLine <= 0 || rawBox.endLine < rawBox.startLine || rawBox.rect.height() <= 0)
            continue;

        const int top = rawBox.rect.top();
        const int bottom = top + rawBox.rect.height();
        const quint64 key = (quint64(rawBox.startLine) << 32) | quint64(rawBox.endLine);
        const auto existing = indexByRange.constFind(key);
        if (existing == indexByRange.cend()) {
            PreviewSourceBox box;
            box.startLine = rawBox.startLine;
            box.endLine = rawBox.endLine;
            box.top = top;
            box.bottom = bottom;
            indexByRange.insert(key, m_previewSourceBoxes.size());
            m_previewSourceBoxes.append(box);
        } else {
            PreviewSourceBox &box = m_previewSourceBoxes[*existing];
            box.top = qMin(box.top, top);
            box.bottom = qMax(box.bottom, bottom);
        }
    }
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

    const PreviewSourceMarker marker = previewMarkerForLine(m_currentSourceLine);
    if (!marker.valid())
    {
        m_previewLineMarker->hide();
        return;
    }

    QWidget *viewport = m_preview->viewport();
    const int viewportY = marker.top - m_preview->verticalScrollBar()->value();
    const int markerHeight = marker.height;
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

MarkdownDocumentPage::PreviewSourceMarker MarkdownDocumentPage::previewMarkerForLine(int line) const
{
    PreviewSourceMarker marker;
    if (line < 1 || m_previewSourceBoxes.isEmpty())
        return marker;

    const PreviewSourceBox *best = nullptr;
    for (const PreviewSourceBox &box : m_previewSourceBoxes) {
        if (line < box.startLine || line > box.endLine || box.height() <= 0)
            continue;
        if (!best || box.lineSpan() < best->lineSpan()
            || (box.lineSpan() == best->lineSpan() && box.height() < best->height())) {
            best = &box;
        }
    }

    const PreviewSourceBox *previous = nullptr;
    const PreviewSourceBox *next = nullptr;
    const auto considerNeighbor = [&](const PreviewSourceBox &box) {
        if (best && (&box == best || box.startLine < best->startLine || box.endLine > best->endLine))
            return;
        if (box.endLine < line
            && (!previous || box.endLine > previous->endLine
                || (box.endLine == previous->endLine && box.bottom > previous->bottom))) {
            previous = &box;
        }
        if (box.startLine > line
            && (!next || box.startLine < next->startLine
                || (box.startLine == next->startLine && box.top < next->top))) {
            next = &box;
        }
    };

    // A separator row or blank line sits inside a parent block (the table, a
    // list item) but between the real children. Mark that gap instead of the
    // whole parent. A paragraph has no children, so the whole paragraph stays
    // the marker.
    const bool parentSpansLines = best && best->lineSpan() > 0;
    if (!best || parentSpansLines) {
        for (const PreviewSourceBox &box : m_previewSourceBoxes)
            considerNeighbor(box);
    }

    auto pinToEdge = [&](int edge) {
        const int markerHeight = qMax(8, m_preview ? m_preview->fontMetrics().height() : 18);
        marker.height = markerHeight;
        marker.top = qMax(0, edge - markerHeight / 2);
        marker.scrollY = edge;
    };

    if (previous && next) {
        const int gapTop = previous->bottom;
        const int gapBottom = qMax(gapTop, next->top);
        const int markerHeight = qMax(8, m_preview ? m_preview->fontMetrics().height() : 18);
        const int gapTravel = gapBottom - gapTop;
        if (gapTravel >= markerHeight) {
            const int firstGapLine = previous->endLine + 1;
            const int lastGapLine = qMax(firstGapLine, next->startLine - 1);
            const int gapLines = lastGapLine - firstGapLine;
            const double t = gapLines <= 0
                                 ? 0.5
                                 : qBound(0.0, double(line - firstGapLine) / double(gapLines), 1.0);
            marker.height = markerHeight;
            marker.top = gapTop + qRound(t * double(gapTravel - markerHeight));
            marker.scrollY = marker.top + markerHeight / 2;
            return marker;
        }
        pinToEdge(next->top);
        return marker;
    }

    if (!best) {
        if (next)
            pinToEdge(next->top);
        else if (previous)
            pinToEdge(previous->bottom);
        return marker;
    }

    marker.top = best->top;
    marker.height = best->height();
    if (best->lineSpan() <= 0) {
        marker.scrollY = best->top + marker.height / 2;
        return marker;
    }

    const double t = qBound(0.0,
                            double(line - best->startLine) / double(best->lineSpan()),
                            1.0);
    marker.scrollY = best->top + qRound(t * double(marker.height));
    return marker;
}

int MarkdownDocumentPage::previewYForSourceLine(int line) const
{
    const PreviewSourceMarker marker = previewMarkerForLine(line);
    return marker.valid() ? marker.scrollY : -1;
}

int MarkdownDocumentPage::sourceLineForPreviewY(int y) const
{
    if (m_previewSourceBoxes.isEmpty())
        return -1;

    const PreviewSourceBox *best = nullptr;
    for (const PreviewSourceBox &box : m_previewSourceBoxes) {
        if (y < box.top || y >= box.bottom || box.height() <= 0)
            continue;
        if (!best || box.height() < best->height()
            || (box.height() == best->height() && box.lineSpan() < best->lineSpan())) {
            best = &box;
        }
    }

    if (!best) {
        const PreviewSourceBox *previous = nullptr;
        const PreviewSourceBox *next = nullptr;
        for (const PreviewSourceBox &box : m_previewSourceBoxes) {
            if (box.bottom <= y && (!previous || box.bottom > previous->bottom))
                previous = &box;
            if (box.top > y && (!next || box.top < next->top))
                next = &box;
        }
        if (previous && next) {
            const int boundary = previous->bottom + (next->top - previous->bottom) / 2;
            return y < boundary ? previous->endLine : next->startLine;
        }
        if (previous)
            return previous->endLine;
        if (next)
            return next->startLine;
        return -1;
    }

    const PreviewSourceBox *previous = nullptr;
    const PreviewSourceBox *next = nullptr;
    bool hasChild = false;
    for (const PreviewSourceBox &box : m_previewSourceBoxes) {
        if (&box == best || box.startLine < best->startLine || box.endLine > best->endLine)
            continue;
        if (box.lineSpan() >= best->lineSpan())
            continue;
        hasChild = true;
        if (box.bottom <= y
            && (!previous || box.endLine > previous->endLine
                || (box.endLine == previous->endLine && box.bottom > previous->bottom))) {
            previous = &box;
        }
        if (box.top > y
            && (!next || box.startLine < next->startLine
                || (box.startLine == next->startLine && box.top < next->top))) {
            next = &box;
        }
    }

    if (hasChild && previous && next && next->startLine > previous->endLine + 1) {
        const int firstGapLine = previous->endLine + 1;
        const int lastGapLine = next->startLine - 1;
        if (lastGapLine <= firstGapLine || next->top <= previous->bottom)
            return firstGapLine;
        const double t = qBound(0.0,
                                double(y - previous->bottom) / double(next->top - previous->bottom),
                                1.0);
        return qBound(firstGapLine, firstGapLine + qRound(t * double(lastGapLine - firstGapLine)), lastGapLine);
    }

    if (best->lineSpan() <= 0)
        return best->startLine;

    const double t = qBound(0.0, double(y - best->top) / double(best->height()), 1.0);
    return qBound(best->startLine,
                  best->startLine + qRound(t * double(best->lineSpan())),
                  best->endLine);
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
