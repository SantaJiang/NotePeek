#include "editor/documentpage.h"

#include "editor/customeditor.h"
#include "tool/markdown/markdownpreviewer.h"

#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
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
    , m_syncingScroll(false)
    , m_mode(SourceOnly)
    , m_homeMode(SourceOnly)
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
    connect(m_splitter, &QSplitter::splitterMoved,
            this, &MarkdownDocumentPage::schedulePreviewScrollMapRebuild);
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
    // From the split view, collapse back to whichever single pane the file
    // opened in (source or preview).  From either single pane, expand to the
    // split view showing both.
    setViewMode(m_mode == Split ? m_homeMode : Split, previewer);
    return isPreviewVisible();
}

void MarkdownDocumentPage::hidePreview()
{
    setViewMode(SourceOnly, nullptr);
}

void MarkdownDocumentPage::setInitialViewMode(bool preview, MarkdownPreviewer *previewer, bool giveFocus)
{
    m_homeMode = preview ? PreviewOnly : SourceOnly;
    setViewMode(m_homeMode, previewer, giveFocus);
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
        break;
    case PreviewOnly:
        m_preview->show();
        if (m_editor)
        {
            m_editor->hide();
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

    if (mode != SourceOnly)
    {
        updatePreview();
        if (giveFocus)
        {
            m_preview->setFocus();
        }
    }
    else if (m_editor && giveFocus)
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
    m_preview->installEventFilter(this);
    m_splitter->addWidget(m_preview);
    m_preview->hide();
}

void MarkdownDocumentPage::updatePreview()
{
    if (!m_previewer || !m_preview || !m_editor || !isPreviewVisible())
    {
        return;
    }

    const int sourceLine = sourceLineAtEditorCenter();
    const double ratio = previewScrollRatio();
    const int firstVisibleLine = int(m_editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE));
    const int xOffset = int(m_editor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET));

    m_syncingScroll = true;
    m_previewer->updatePreview(m_preview, m_editor->text(), m_editor->filePath());
    rebuildPreviewScrollMap();
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
    m_editor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);

    QTimer::singleShot(0, this, [this, sourceLine, ratio, firstVisibleLine, xOffset]() {
        if (!isPreviewVisible() || !m_preview || !m_editor)
        {
            m_syncingScroll = false;
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

    static const QRegularExpression anchorPattern(QStringLiteral("id=\"np-src-line-(\\d+)\""));
    QSet<int> seenLines;
    QRegularExpressionMatchIterator it = anchorPattern.globalMatch(m_preview->html());
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        const int line = match.captured(1).toInt();
        if (seenLines.contains(line))
        {
            continue;
        }

        const int y = m_preview->anchorY(QStringLiteral("np-src-line-%1").arg(line));
        if (y >= 0)
        {
            m_previewLineMap.append(qMakePair(line, y));
            seenLines.insert(line);
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

    const int sourceLine = sourceLineAtEditorCenter();
    QTimer::singleShot(0, this, [this, sourceLine]() {
        if (!isPreviewVisible() || !m_preview)
        {
            return;
        }

        rebuildPreviewScrollMap();
        const int y = previewYForSourceLine(sourceLine);
        if (y >= 0 && !m_syncingScroll)
        {
            m_syncingScroll = true;
            QScrollBar *bar = m_preview->verticalScrollBar();
            const int centeredY = y - m_preview->viewport()->height() / 2;
            bar->setValue(qBound(bar->minimum(), centeredY, bar->maximum()));
            m_syncingScroll = false;
        }
    });
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

    if (line <= m_previewLineMap.first().first)
    {
        return m_previewLineMap.first().second;
    }

    for (int i = 1; i < m_previewLineMap.size(); ++i)
    {
        const auto previous = m_previewLineMap.at(i - 1);
        const auto next = m_previewLineMap.at(i);
        if (line <= next.first)
        {
            const int lineSpan = next.first - previous.first;
            if (lineSpan <= 0)
            {
                return previous.second;
            }

            const double t = double(line - previous.first) / double(lineSpan);
            return previous.second + qRound(t * double(next.second - previous.second));
        }
    }

    return m_previewLineMap.last().second;
}

int MarkdownDocumentPage::sourceLineForPreviewY(int y) const
{
    if (m_previewLineMap.isEmpty())
    {
        return -1;
    }

    if (y <= m_previewLineMap.first().second)
    {
        return m_previewLineMap.first().first;
    }

    for (int i = 1; i < m_previewLineMap.size(); ++i)
    {
        const auto previous = m_previewLineMap.at(i - 1);
        const auto next = m_previewLineMap.at(i);
        if (y <= next.second)
        {
            const int ySpan = next.second - previous.second;
            if (ySpan <= 0)
            {
                return previous.first;
            }

            const double t = double(y - previous.second) / double(ySpan);
            return previous.first + qRound(t * double(next.first - previous.first));
        }
    }

    return m_previewLineMap.last().first;
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
