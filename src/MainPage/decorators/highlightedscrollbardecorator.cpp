#include "highlightedscrollbardecorator.h"
#include "highlightedscrollbar.h"
#include <Qsci/qsciscintilla.h>

HighlightedScrollBarDecorator::HighlightedScrollBarDecorator(QsciScintilla *editor)
    : QObject(editor)
    , m_editor(editor)
    , m_scrollBar(new HighlightedScrollBar(editor, Qt::Vertical, editor))
{
    m_searchIndicator = editor->indicatorDefine(QsciScintilla::RoundBoxIndicator);
    m_bookmarkMarker  = editor->markerDefine(QsciScintilla::Circle);

    editor->setIndicatorForegroundColor(QColor(255, 200, 0), m_searchIndicator);
    editor->setMarkerBackgroundColor(QColor(100, 100, 255), m_bookmarkMarker);
    editor->setMarkerForegroundColor(Qt::white, m_bookmarkMarker);

    m_scrollBar->setSearchIndicator(m_searchIndicator);
    m_scrollBar->setBookmarkMarker(m_bookmarkMarker);

    editor->setVerticalScrollBar(m_scrollBar);

    connect(editor, SIGNAL(textChanged()),
            this, SLOT(handleTextChanged()));
    connect(editor, SIGNAL(cursorPositionChanged(int, int)),
            this, SLOT(handleCursorPositionChanged(int, int)));
    connect(editor, SIGNAL(selectionChanged()),
            this, SLOT(handleSelectionChanged()));
    connect(m_scrollBar, &QScrollBar::valueChanged,
            this, &HighlightedScrollBarDecorator::handleScroll);
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
    updateScrollBar();
}

void HighlightedScrollBarDecorator::updateScrollBar()
{
    if (m_scrollBar) {
        m_scrollBar->updateScrollBarFromEditor();
        m_scrollBar->update();
    }
}
