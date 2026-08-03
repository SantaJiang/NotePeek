#ifndef HIGHLIGHTEDSCROLLBAR_H
#define HIGHLIGHTEDSCROLLBAR_H

#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QVector>
#include <QWidget>

class QsciScintilla;

// Transparent marker layer placed over QScintilla's original vertical
// scrollbar.  Keeping the native scrollbar avoids the expensive full-widget
// repaint path triggered by replacing it on macOS while preserving all marker
// visuals and native mouse/trackpad interaction.
class HighlightedScrollBar : public QWidget
{
    Q_OBJECT

public:
    // The layer only ever tracks the editor's vertical scrollbar, so it takes
    // no orientation: accepting one would let callers ask for a horizontal
    // layer that silently behaves as a vertical one.
    explicit HighlightedScrollBar(QsciScintilla *editor, QWidget *parent = nullptr);
    ~HighlightedScrollBar();

    void setSearchIndicator(int indicator) { searchIndicator = indicator; }
    void setBookmarkMarker(int marker) { bookmarkMarker = marker; }
    void updateScrollBarFromEditor();
    void refreshSearchMarkers();
    void refreshBookmarkMarkers();
    void applyThemeColors();
    void setCurrentLine(int line);
    int searchMarkerCount() const { return m_searchLines.size(); }
    int bookmarkMarkerCount() const { return m_bookmarkLines.size(); }
    QVector<int> searchMarkerLines() const { return m_searchLines; }
    QVector<int> bookmarkMarkerLines() const { return m_bookmarkLines; }
    int currentLine() const { return m_currentLine; }

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void drawSearchResults(QPainter &p);
    void drawBookmarks(QPainter &p);
    void drawCurrentLine(QPainter &p);
    void drawTickMark(QPainter &p, int y, int height, const QColor &color);
    void rebuildStaticMarkerLayer();
    void syncToNativeScrollBar();
    int lineToScrollBarY(int line) const;
    // The track the slider actually travels in, taken from the style rather
    // than assumed to be the widget minus two square arrow buttons: macOS
    // overlay scrollbars have no arrows, so that assumption shrank the usable
    // range by twice the scrollbar width and pushed every marker off target.
    QRect grooveRect() const;

    QsciScintilla *editor;
    QPointer<QWidget> m_nativeScrollBar;

    int searchIndicator = -1;
    int bookmarkMarker = -1;
    int m_lineCount = 1;
    int m_currentLine = 0;
    QColor m_searchColor;
    QColor m_bookmarkColor;
    QColor m_currentLineColor;
    QPixmap m_staticMarkerLayer;
    bool m_staticMarkerLayerDirty = true;
    QVector<int> m_searchLines;
    QVector<int> m_bookmarkLines;

    // Tick mark dimensions
    static constexpr int DEFAULT_TICK_HEIGHT = 3;
    static constexpr int DEFAULT_TICK_PADDING = 3;
};

#endif // HIGHLIGHTEDSCROLLBAR_H
