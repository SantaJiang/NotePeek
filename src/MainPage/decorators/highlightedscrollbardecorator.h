#ifndef HIGHLIGHTEDSCROLLBARDECORATOR_H
#define HIGHLIGHTEDSCROLLBARDECORATOR_H

#include "editordecorator.h"
#include <QPointer>

class HighlightedScrollBar;
class FindReplaceWidget;

class HighlightedScrollBarDecorator : public EditorDecorator
{
    Q_OBJECT

public:
    explicit HighlightedScrollBarDecorator(QsciScintilla *editor);

    void notify(const void *notificationData = nullptr) override;

    HighlightedScrollBar *getScrollBar() const { return scrollBar; }
    int getSearchIndicator() const { return searchIndicator; }

public slots:
    void updateScrollBar();

private slots:
    void handleTextChanged();
    void handleCursorPositionChanged(int line, int index);
    void handleSelectionChanged();
    void handleScroll(int value);

private:
    HighlightedScrollBar *scrollBar;
    int searchIndicator;
    int bookmarkMarker;
};

#endif // HIGHLIGHTEDSCROLLBARDECORATOR_H
