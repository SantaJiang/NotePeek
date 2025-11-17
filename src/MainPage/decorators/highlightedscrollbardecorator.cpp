#include "highlightedscrollbardecorator.h"
#include "highlightedscrollbar.h"
#include <Qsci/qsciscintilla.h>
#include <QDebug>

HighlightedScrollBarDecorator::HighlightedScrollBarDecorator(QsciScintilla *editor)
    : EditorDecorator(editor),
    scrollBar(new HighlightedScrollBar(editor, Qt::Vertical, editor))
{
    // 分配指示器和标记
    searchIndicator = editor->indicatorDefine(QsciScintilla::RoundBoxIndicator);
    bookmarkMarker = editor->markerDefine(QsciScintilla::Circle);

    // 设置颜色
    editor->setIndicatorForegroundColor(QColor(255, 200, 0), searchIndicator);
    editor->setMarkerBackgroundColor(QColor(100, 100, 255), bookmarkMarker);
    editor->setMarkerForegroundColor(Qt::white, bookmarkMarker);

    // 配置滚动条
    scrollBar->setSearchIndicator(searchIndicator);
    scrollBar->setBookmarkMarker(bookmarkMarker);

    // 替换编辑器的垂直滚动条
    editor->setVerticalScrollBar(scrollBar);

    // 连接编辑器信号
    connect(editor, SIGNAL(textChanged()), this, SLOT(handleTextChanged()));
    connect(editor, SIGNAL(cursorPositionChanged(int, int)), this, SLOT(handleCursorPositionChanged(int, int)));
    connect(editor, SIGNAL(selectionChanged()), this, SLOT(handleSelectionChanged()));

    // 连接滚动条同步
    connect(scrollBar, &QScrollBar::valueChanged, this, &HighlightedScrollBarDecorator::handleScroll);
}


void HighlightedScrollBarDecorator::notify(const void *notificationData)
{
    Q_UNUSED(notificationData)
    updateScrollBar();
}

void HighlightedScrollBarDecorator::handleTextChanged()
{
    updateScrollBar();
}

void HighlightedScrollBarDecorator::handleCursorPositionChanged(int line, int index)
{
    Q_UNUSED(line)
    Q_UNUSED(index)
    updateScrollBar();
}

void HighlightedScrollBarDecorator::handleSelectionChanged()
{
    updateScrollBar();
}

void HighlightedScrollBarDecorator::handleScroll(int value)
{
    Q_UNUSED(value)
    // 滚动事件已由滚动条处理，这里只需更新显示
    updateScrollBar();
}

void HighlightedScrollBarDecorator::updateScrollBar()
{
    if (scrollBar) {
        scrollBar->updateScrollBarFromEditor();
        scrollBar->update();
    }
}
