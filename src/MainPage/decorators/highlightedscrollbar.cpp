#include "decorators/highlightedscrollbar.h"
#include "themecolors.h"
#include <Qsci/qsciscintilla.h>
#include <QEvent>
#include <QScrollBar>
#include <QTimer>
#include <utility>

HighlightedScrollBar::HighlightedScrollBar(QsciScintilla *editor, Qt::Orientation orientation, QWidget *parent)
    : QWidget(editor ? static_cast<QWidget *>(editor->verticalScrollBar()) : parent)
    , editor(editor)
    , m_nativeScrollBar(editor ? editor->verticalScrollBar() : nullptr)
{
    Q_UNUSED(orientation)
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    if (m_nativeScrollBar)
    {
        m_nativeScrollBar->installEventFilter(this);
        syncToNativeScrollBar();
    }
    applyThemeColors();

    // Defer initial sync until the editor is fully set up
    QTimer::singleShot(0, this, &HighlightedScrollBar::updateScrollBarFromEditor);
}

HighlightedScrollBar::~HighlightedScrollBar()
{
}

void HighlightedScrollBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    if (m_staticMarkerLayerDirty
        || m_staticMarkerLayer.size() != size() * devicePixelRatioF())
        rebuildStaticMarkerLayer();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.drawPixmap(0, 0, m_staticMarkerLayer);
    drawCurrentLine(p);
}

bool HighlightedScrollBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_nativeScrollBar
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
    {
        // The marker layer is a child of the native scrollbar, so movement is
        // inherited every frame during macOS window animations. Only its local
        // size needs updating when the native scrollbar itself is resized.
        syncToNativeScrollBar();
        m_staticMarkerLayerDirty = true;
        update();
    }
    return QWidget::eventFilter(watched, event);
}

void HighlightedScrollBar::updateScrollBarFromEditor()
{
    if (!editor) return;

    const int lineCount = editor->lines();
    if (m_lineCount != lineCount)
    {
        m_lineCount = lineCount;
        m_staticMarkerLayerDirty = true;
    }
    int cursorIndex = 0;
    editor->getCursorPosition(&m_currentLine, &cursorIndex);
    syncToNativeScrollBar();

    // Repaint to refresh the markers
    raise();
    update();
}

void HighlightedScrollBar::drawSearchResults(QPainter &p)
{
    for (int line : std::as_const(m_searchLines)) {
        drawTickMark(p, lineToScrollBarY(line), DEFAULT_TICK_HEIGHT, m_searchColor);
    }
}

void HighlightedScrollBar::drawBookmarks(QPainter &p)
{
    for (int line : std::as_const(m_bookmarkLines)) {
        drawTickMark(p, lineToScrollBarY(line), DEFAULT_TICK_HEIGHT, m_bookmarkColor);
    }
}

void HighlightedScrollBar::applyThemeColors()
{
    m_searchColor = ThemeColors::ScrollBar::searchResult();
    m_bookmarkColor = ThemeColors::ScrollBar::bookmark();
    m_currentLineColor = ThemeColors::ScrollBar::currentLine();
    m_staticMarkerLayerDirty = true;
    update();
}

void HighlightedScrollBar::setCurrentLine(int line)
{
    if (m_currentLine == line)
        return;
    m_currentLine = line;
    update();
}

void HighlightedScrollBar::refreshSearchMarkers()
{
    m_searchLines.clear();
    m_staticMarkerLayerDirty = true;
    if (!editor || searchIndicator == -1)
    {
        update();
        return;
    }

    const int length = editor->SendScintilla(QsciScintillaBase::SCI_GETLENGTH);
    int currentPos = 0;
    while (currentPos < length)
    {
        if (editor->SendScintilla(QsciScintillaBase::SCI_INDICATORVALUEAT,
                                  searchIndicator, currentPos) == 0)
            currentPos = int(editor->SendScintilla(
                QsciScintillaBase::SCI_INDICATOREND, searchIndicator, currentPos));
        if (currentPos < 0 || currentPos >= length)
            break;

        const int endPos = int(editor->SendScintilla(
            QsciScintillaBase::SCI_INDICATOREND, searchIndicator, currentPos));
        if (endPos <= currentPos)
            break;
        m_searchLines.append(int(editor->SendScintilla(
            QsciScintillaBase::SCI_LINEFROMPOSITION, currentPos)));
        currentPos = endPos;
    }
    update();
}

void HighlightedScrollBar::refreshBookmarkMarkers()
{
    m_bookmarkLines.clear();
    m_staticMarkerLayerDirty = true;
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
    drawTickMark(p, lineToScrollBarY(m_currentLine), DEFAULT_TICK_HEIGHT, m_currentLineColor);
}

void HighlightedScrollBar::rebuildStaticMarkerLayer()
{
    const qreal pixelRatio = devicePixelRatioF();
    m_staticMarkerLayer = QPixmap(size() * pixelRatio);
    m_staticMarkerLayer.setDevicePixelRatio(pixelRatio);
    m_staticMarkerLayer.fill(Qt::transparent);
    QPainter markerPainter(&m_staticMarkerLayer);
    markerPainter.setRenderHint(QPainter::Antialiasing, false);
    drawSearchResults(markerPainter);
    drawBookmarks(markerPainter);
    m_staticMarkerLayerDirty = false;
}

void HighlightedScrollBar::syncToNativeScrollBar()
{
    if (!m_nativeScrollBar)
        return;
    setGeometry(m_nativeScrollBar->rect());
    raise();
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
    if (m_lineCount <= 0) return 0;

    int arrowHeight = scrollbarArrowHeight();
    int availableHeight = rect().height() - arrowHeight * 2;

    double ratio = static_cast<double>(line) / m_lineCount;
    return static_cast<int>(ratio * availableHeight);
}

int HighlightedScrollBar::scrollbarArrowHeight() const
{
    // Assume square arrow buttons: height equals the scrollbar width
    return m_nativeScrollBar ? m_nativeScrollBar->width() : rect().width();
}
