#include "decorators/highlightedscrollbar.h"
#include "themecolors.h"
#include <Qsci/qsciscintilla.h>
#include <QEvent>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTimer>
#include <utility>

HighlightedScrollBar::HighlightedScrollBar(QsciScintilla *editor, QWidget *parent)
    : QWidget(editor ? static_cast<QWidget *>(editor->verticalScrollBar()) : parent)
    , editor(editor)
    , m_nativeScrollBar(editor ? editor->verticalScrollBar() : nullptr)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
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
    if (watched != m_nativeScrollBar)
        return QWidget::eventFilter(watched, event);

    switch (event->type())
    {
    case QEvent::Resize:
    case QEvent::Show:
        // The marker layer is a child of the native scrollbar, so movement is
        // inherited every frame during macOS window animations. Only its local
        // size needs updating when the native scrollbar itself is resized.
        syncToNativeScrollBar();
        m_staticMarkerLayerDirty = true;
        update();
        break;
    // Hide needs no handling: Qt hides child widgets together with their
    // parent, and calling hide() here would set the explicit-hide flag that
    // stops the layer from reappearing when the scrollbar is shown again.
    default:
        break;
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
    // A widget created with an already-visible parent stays hidden until it is
    // shown explicitly, so without this the markers only appear when the
    // decorator happens to be constructed before the editor's first show().
    // Only ever show here: calling hide() while the parent is still hidden
    // would set the explicit-hide flag and stop Qt from showing the layer
    // together with the scrollbar later on.
    if (m_nativeScrollBar->isVisible())
        show();
}

void HighlightedScrollBar::drawTickMark(QPainter &p, int y, int height, const QColor &color)
{
    const QRect groove = grooveRect();
    const int tickHeight = qMax(1, height);
    y = qBound(groove.top(), y, qMax(groove.top(), groove.bottom() - tickHeight + 1));

    p.fillRect(rect().x() + DEFAULT_TICK_PADDING,
               y,
               rect().width() - (DEFAULT_TICK_PADDING * 2),
               tickHeight, color);
}

int HighlightedScrollBar::lineToScrollBarY(int line) const
{
    if (m_lineCount <= 0) return 0;

    const QRect groove = grooveRect();
    const double ratio = qBound(0.0, static_cast<double>(line) / m_lineCount, 1.0);
    return groove.top() + static_cast<int>(ratio * qMax(0, groove.height() - DEFAULT_TICK_HEIGHT));
}

QRect HighlightedScrollBar::grooveRect() const
{
    auto *bar = qobject_cast<QScrollBar *>(m_nativeScrollBar.data());
    if (!bar)
        return rect();

    QStyleOptionSlider option;
    option.initFrom(bar);
    option.orientation = Qt::Vertical;
    option.minimum = bar->minimum();
    option.maximum = bar->maximum();
    option.sliderPosition = bar->value();
    option.sliderValue = bar->value();
    option.pageStep = bar->pageStep();
    option.singleStep = bar->singleStep();
    const QRect groove = bar->style()->subControlRect(QStyle::CC_ScrollBar, &option,
                                                      QStyle::SC_ScrollBarGroove, bar);
    return groove.isValid() ? groove : rect();
}
