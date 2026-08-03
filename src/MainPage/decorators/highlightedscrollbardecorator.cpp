#include "decorators/highlightedscrollbardecorator.h"
#include "decorators/highlightedscrollbar.h"
#include "themecolors.h"
#include <QTimer>
#include <QScrollBar>
#include <Qsci/qsciscintilla.h>

HighlightedScrollBarDecorator::HighlightedScrollBarDecorator(QsciScintilla *editor)
    : QObject(editor)
    , m_editor(editor)
    , m_scrollBar(new HighlightedScrollBar(editor))
{
    m_searchIndicator = editor->indicatorDefine(QsciScintilla::RoundBoxIndicator);
    m_bookmarkMarker  = editor->markerDefine(QsciScintilla::Circle);

    applyThemeColors();

    m_scrollBar->setSearchIndicator(m_searchIndicator);
    m_scrollBar->setBookmarkMarker(m_bookmarkMarker);

    connect(editor, SIGNAL(textChanged()),
            this, SLOT(handleTextChanged()));
    connect(editor, SIGNAL(cursorPositionChanged(int, int)),
            this, SLOT(handleCursorPositionChanged(int, int)));
}

void HighlightedScrollBarDecorator::notify(const void *notificationData)
{
    Q_UNUSED(notificationData)
    if (m_scrollBar)
    {
        m_scrollBar->refreshSearchMarkers();
        m_scrollBar->refreshBookmarkMarkers();
    }
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
    m_scrollBar->applyThemeColors();

    updateScrollBar();
}

void HighlightedScrollBarDecorator::handleTextChanged()
{
    updateScrollBar();
}

void HighlightedScrollBarDecorator::handleCursorPositionChanged(int line, int index)
{
    Q_UNUSED(index)
    if (m_scrollBar)
        m_scrollBar->setCurrentLine(line);
}

void HighlightedScrollBarDecorator::updateScrollBar()
{
    // Every keystroke, cursor move, selection change and scroll lands here;
    // coalesce the bursts so one event-loop pass repaints the bar only once.
    if (m_updatePending)
    {
        return;
    }
    m_updatePending = true;
    QTimer::singleShot(0, this, [this]() {
        m_updatePending = false;
        if (m_scrollBar) {
            m_scrollBar->updateScrollBarFromEditor();
            m_scrollBar->update();
        }
    });
}
