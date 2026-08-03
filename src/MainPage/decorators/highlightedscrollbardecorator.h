#ifndef HIGHLIGHTEDSCROLLBARDECORATOR_H
#define HIGHLIGHTEDSCROLLBARDECORATOR_H

#include <QObject>

class QsciScintilla;
class HighlightedScrollBar;

class HighlightedScrollBarDecorator : public QObject
{
    Q_OBJECT

public:
    explicit HighlightedScrollBarDecorator(QsciScintilla *editor);

    void notify(const void *notificationData = nullptr);
    void applyThemeColors();

    HighlightedScrollBar *getScrollBar() const { return m_scrollBar; }
    int getSearchIndicator() const { return m_searchIndicator; }
    int getBookmarkMarker() const { return m_bookmarkMarker; }

public slots:
    void updateScrollBar();

private slots:
    void handleTextChanged();
    void handleCursorPositionChanged(int line, int index);

private:
    QsciScintilla *m_editor;
    HighlightedScrollBar *m_scrollBar;
    int m_searchIndicator;
    int m_bookmarkMarker;
    bool m_updatePending = false;
};

#endif // HIGHLIGHTEDSCROLLBARDECORATOR_H
