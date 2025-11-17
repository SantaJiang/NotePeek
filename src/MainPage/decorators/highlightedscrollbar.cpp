#include "highlightedscrollbar.h"
#include <Qsci/qsciscintilla.h>
#include <QDebug>
#include "../find/findreplacewidget.h"

HighlightedScrollBar::HighlightedScrollBar(QsciScintilla *editor, Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent),
    editor(editor)
{
    // 连接滚动条和编辑器的同步
    connect(this, &QScrollBar::valueChanged, this, &HighlightedScrollBar::onValueChanged);
    connect(this, &QScrollBar::rangeChanged, this, &HighlightedScrollBar::onRangeChanged);

    // 初始同步
    QTimer::singleShot(0, this, &HighlightedScrollBar::updateScrollBarFromEditor);
}

HighlightedScrollBar::~HighlightedScrollBar()
{
}

void HighlightedScrollBar::paintEvent(QPaintEvent *event)
{
    // 先绘制默认滚动条
    QScrollBar::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // 绘制各种标记
    drawSearchResults(p);
    drawBookmarks(p);
    drawCurrentLine(p);
}

void HighlightedScrollBar::onValueChanged(int value)
{
    if (!editor || m_blockSignals) return;

    // 使用 QScintilla 的原始 API 设置滚动位置
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

    // 获取编辑器的滚动信息
    int firstVisibleLine = editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
    int linesOnScreen = editor->SendScintilla(QsciScintillaBase::SCI_LINESONSCREEN);
    int lineCount = editor->lines();

    // 更新滚动条范围
    setMinimum(0);
    setMaximum(qMax(0, lineCount - linesOnScreen));
    setPageStep(linesOnScreen);
    setValue(firstVisibleLine);

    m_blockSignals = false;

    // 触发重绘以更新标记
    update();
}

void HighlightedScrollBar::drawSearchResults(QPainter &p)
{
    if (searchIndicator == -1) return;

    // 查找所有使用搜索指示器的位置
    int length = editor->length();
    int currentPos = 0;

    while (currentPos < length) {
        int startPos = editor->SendScintilla(QsciScintillaBase::SCI_INDICATORSTART, searchIndicator, currentPos);
        int endPos = editor->SendScintilla(QsciScintillaBase::SCI_INDICATOREND, searchIndicator, currentPos);

        if (startPos == -1 || endPos == -1 || startPos >= endPos) break;

        drawTickMark(p, posToScrollBarY(startPos), DEFAULT_TICK_HEIGHT, SEARCH_RESULT_COLOR);
        currentPos = endPos + 1;
    }
}

void HighlightedScrollBar::drawBookmarks(QPainter &p)
{
    if (bookmarkMarker == -1) return;

    // 查找所有书签
    int line = 0;
    while ((line = editor->markerFindNext(line, 1 << bookmarkMarker)) != -1) {
        drawTickMark(p, lineToScrollBarY(line), DEFAULT_TICK_HEIGHT, BOOKMARK_COLOR);
        line++;
    }
}

void HighlightedScrollBar::drawCurrentLine(QPainter &p)
{
    int line, index;
    editor->getCursorPosition(&line, &index);
    drawTickMark(p, lineToScrollBarY(line), DEFAULT_TICK_HEIGHT, CURRENT_LINE_COLOR);
}

void HighlightedScrollBar::drawTickMark(QPainter &p, int y, int height, const QColor &color)
{
    int arrowHeight = scrollbarArrowHeight();
    int availableHeight = rect().height() - arrowHeight * 2;

    // 确保y在有效范围内
    if (y < 0) y = 0;
    if (y > availableHeight) y = availableHeight;

    p.fillRect(rect().x() + DEFAULT_TICK_PADDING,
               y + arrowHeight,
               rect().width() - (DEFAULT_TICK_PADDING * 2),
               qMax(1, height), color);
}

int HighlightedScrollBar::posToScrollBarY(int pos) const
{
    // 在 QScintilla 中，我们需要使用 SendScintilla 来获取位置对应的行号
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
    // 假设箭头是正方形，高度等于滚动条宽度
    return rect().width();
}
