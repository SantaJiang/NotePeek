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

    // 常量定义
    const int DEFAULT_TICK_HEIGHT = 3;
    const int DEFAULT_TICK_PADDING = 3;
    const QColor SEARCH_RESULT_COLOR = QColor(255, 200, 0, 100);    // 橙色
    const QColor BOOKMARK_COLOR = QColor(100, 100, 255, 180);       // 蓝色
    const QColor CURRENT_LINE_COLOR = QColor(0, 0, 0, 100);         // 半透明黑
    const QColor SELECTION_COLOR = QColor(0, 0, 0, 25);            // 选区颜色
};

#endif // HIGHLIGHTEDSCROLLBAR_H
