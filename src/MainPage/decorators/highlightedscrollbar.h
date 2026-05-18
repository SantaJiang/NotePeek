#ifndef HIGHLIGHTEDSCROLLBAR_H
#define HIGHLIGHTEDSCROLLBAR_H

#include <QScrollBar>
#include <QPainter>

class QsciScintilla;

class HighlightedScrollBar : public QScrollBar
{
    Q_OBJECT

public:
    explicit HighlightedScrollBar(QsciScintilla *editor, Qt::Orientation orientation, QWidget *parent = nullptr);
    ~HighlightedScrollBar();

    void setSearchIndicator(int indicator) { searchIndicator = indicator; }
    void setBookmarkMarker(int marker) { bookmarkMarker = marker; }
    void updateScrollBarFromEditor();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onValueChanged(int value);
    void onRangeChanged(int min, int max);

private:
    void drawSearchResults(QPainter &p);
    void drawBookmarks(QPainter &p);
    void drawCurrentLine(QPainter &p);
    void drawTickMark(QPainter &p, int y, int height, const QColor &color);
    int posToScrollBarY(int pos) const;
    int lineToScrollBarY(int line) const;
    int scrollbarArrowHeight() const;

    QsciScintilla *editor;

    int searchIndicator = -1;
    int bookmarkMarker = -1;
    bool m_blockSignals = false;

    // Tick mark dimensions
    static constexpr int DEFAULT_TICK_HEIGHT = 3;
    static constexpr int DEFAULT_TICK_PADDING = 3;
};

#endif // HIGHLIGHTEDSCROLLBAR_H
