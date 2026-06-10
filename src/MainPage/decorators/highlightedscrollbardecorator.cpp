#include "decorators/highlightedscrollbardecorator.h"
#include "decorators/highlightedscrollbar.h"
#include "themecolors.h"
#include <Qsci/qsciscintilla.h>

HighlightedScrollBarDecorator::HighlightedScrollBarDecorator(QsciScintilla *editor)
    : QObject(editor)
    , m_editor(editor)
    , m_scrollBar(new HighlightedScrollBar(editor, Qt::Vertical, editor))
{
    m_searchIndicator = editor->indicatorDefine(QsciScintilla::RoundBoxIndicator);
    m_bookmarkMarker  = editor->markerDefine(QsciScintilla::Circle);

    applyThemeColors();

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

void HighlightedScrollBarDecorator::applyThemeColors()
{
    if (!m_editor)
    {
        return;
    }

    QColor searchHighlightColor = ThemeColors::ScrollBar::searchResult();
    m_editor->setIndicatorForegroundColor(searchHighlightColor, m_searchIndicator);
    m_editor->setMarkerBackgroundColor(ThemeColors::ScrollBar::bookmark(), m_bookmarkMarker);
    m_editor->setMarkerForegroundColor(Qt::white, m_bookmarkMarker);

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
