#include "decorators/highlightedscrollbar.h"
#include "themecolors.h"
#include <Qsci/qsciscintilla.h>
#include <QTimer>
#include <utility>

HighlightedScrollBar::HighlightedScrollBar(QsciScintilla *editor, Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent),
    editor(editor)
{
    // Keep the scrollbar and editor in sync
    connect(this, &QScrollBar::valueChanged, this, &HighlightedScrollBar::onValueChanged);
    connect(this, &QScrollBar::rangeChanged, this, &HighlightedScrollBar::onRangeChanged);

    // Defer initial sync until the editor is fully set up
    QTimer::singleShot(0, this, &HighlightedScrollBar::updateScrollBarFromEditor);
}

HighlightedScrollBar::~HighlightedScrollBar()
{
}

void HighlightedScrollBar::paintEvent(QPaintEvent *event)
{
    // Draw the default scrollbar first, then overlay the markers
    QScrollBar::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    drawSearchResults(p);
    drawBookmarks(p);
    drawCurrentLine(p);
}

void HighlightedScrollBar::onValueChanged(int value)
{
    if (!editor || m_blockSignals) return;

    // Scroll the editor through the raw Scintilla API
    editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, value);
}

void HighlightedScrollBar::onRangeChanged(int min, int max)
{
    Q_UNUSED(min)
    Q_UNUSED(max)
    updateScrollBarFromEditor();
}

void HighlightedScrollBar::updateScrollBarFromEditor()
{
    if (!editor) return;

    m_blockSignals = true;

    int firstVisibleLine = editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
    int linesOnScreen = editor->SendScintilla(QsciScintillaBase::SCI_LINESONSCREEN);
    int lineCount = editor->lines();

    setMinimum(0);
    setMaximum(qMax(0, lineCount - linesOnScreen));
    setPageStep(linesOnScreen);
    setValue(firstVisibleLine);

    m_blockSignals = false;

    // Repaint to refresh the markers
    update();
}

void HighlightedScrollBar::drawSearchResults(QPainter &p)
{
    for (int line : std::as_const(m_searchLines)) {
        drawTickMark(p, lineToScrollBarY(line), DEFAULT_TICK_HEIGHT, ThemeColors::ScrollBar::searchResult());
    }
}

void HighlightedScrollBar::drawBookmarks(QPainter &p)
{
    for (int line : std::as_const(m_bookmarkLines)) {
        drawTickMark(p, lineToScrollBarY(line), DEFAULT_TICK_HEIGHT, ThemeColors::ScrollBar::bookmark());
    }
}

void HighlightedScrollBar::refreshSearchMarkers()
{
    m_searchLines.clear();
    if (!editor || searchIndicator == -1)
    {
        update();
        return;
    }

    const int length = editor->SendScintilla(QsciScintillaBase::SCI_GETLENGTH);
    int currentPos = 0;
    while (currentPos < length)
    {
        const int startPos = editor->SendScintilla(QsciScintillaBase::SCI_INDICATORSTART, searchIndicator, currentPos);
        const int endPos = editor->SendScintilla(QsciScintillaBase::SCI_INDICATOREND, searchIndicator, currentPos);
        if (startPos == -1 || endPos == -1 || startPos >= endPos)
        {
            break;
        }

        m_searchLines.append(editor->SendScintilla(QsciScintillaBase::SCI_LINEFROMPOSITION, startPos));
        currentPos = endPos + 1;
    }
    update();
}

void HighlightedScrollBar::refreshBookmarkMarkers()
{
    m_bookmarkLines.clear();
    if (!editor || bookmarkMarker == -1)
    {
        update();
        return;
    }

    int line = 0;
    while ((line = editor->markerFindNext(line, 1 << bookmarkMarker)) != -1)
    {
        m_bookmarkLines.append(line);
        ++line;
    }
    update();
}

void HighlightedScrollBar::drawCurrentLine(QPainter &p)
{
    int line, index;
    editor->getCursorPosition(&line, &index);
    drawTickMark(p, lineToScrollBarY(line), DEFAULT_TICK_HEIGHT, ThemeColors::ScrollBar::currentLine());
}

void HighlightedScrollBar::drawTickMark(QPainter &p, int y, int height, const QColor &color)
{
    int arrowHeight = scrollbarArrowHeight();
    int availableHeight = rect().height() - arrowHeight * 2;

    // Clamp y to the drawable area
    if (y < 0) y = 0;
    if (y > availableHeight) y = availableHeight;

    p.fillRect(rect().x() + DEFAULT_TICK_PADDING,
               y + arrowHeight,
               rect().width() - (DEFAULT_TICK_PADDING * 2),
               qMax(1, height), color);
}

int HighlightedScrollBar::posToScrollBarY(int pos) const
{
    // QScintilla only exposes position-to-line mapping via SendScintilla
    int line = editor->SendScintilla(QsciScintillaBase::SCI_LINEFROMPOSITION, pos);
    return lineToScrollBarY(line);
}

int HighlightedScrollBar::lineToScrollBarY(int line) const
{
    int lineCount = editor->lines();
    if (lineCount <= 0) return 0;

    int arrowHeight = scrollbarArrowHeight();
    int availableHeight = rect().height() - arrowHeight * 2;

    double ratio = static_cast<double>(line) / lineCount;
    return static_cast<int>(ratio * availableHeight);
}

int HighlightedScrollBar::scrollbarArrowHeight() const
{
    // Assume square arrow buttons: height equals the scrollbar width
    return rect().width();
}
