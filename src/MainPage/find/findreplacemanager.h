#ifndef FINDREPLACEMANAGER_H
#define FINDREPLACEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QColor>

class CustomEditor;
class HighlightedScrollBarDecorator;

class FindReplaceManager : public QObject
{
    Q_OBJECT

public:
    struct SearchResult {
        int position;
        int length;
        QString text;
    };

    explicit FindReplaceManager(QObject *parent = nullptr);
    ~FindReplaceManager();

    void setCurrentEditor(CustomEditor *editor);

    QString findText() const { return m_findText; }
    QString replaceText() const { return m_replaceText; }
    bool caseSensitive() const { return m_caseSensitive; }
    bool wholeWord() const { return m_wholeWord; }
    bool wrapAround() const { return m_wrapAround; }
    bool backwards() const { return m_backwards; }

    QList<SearchResult> searchResults() const { return m_searchResults; }

    void find(const QString &text = QString());
    void findNext();
    void replace(const QString &replaceText = QString());
    void replaceAll();
    void highlightAllMatches();
    void clearHighlights();
    void refreshHighlights();

    void setFindText(const QString &text);
    void setReplaceText(const QString &text);
    void setCaseSensitive(bool on);
    void setWholeWord(bool on);
    void setWrapAround(bool on);
    void setBackwards(bool backwards);

signals:
    void searchResultsChanged();
    void findRequested();
    void replaceRequested();
    void replaceAllRequested();
    void highlightsRefreshed();

private:
    void addToFindHistory(const QString &text);
    void addToReplaceHistory(const QString &text);
    void selectFirstMatch();
    void updateScrollBarMarkers();
    void initializeEditor();
    bool isLargeFile() const;
    void prepareFindOptions(bool &re, bool &cs, bool &wo, bool &wrap, bool &forward, bool wrapAround = false) const;

    static constexpr qint64 LARGE_FILE_SIZE = 10 * 1024 * 1024; // 10 MB

    CustomEditor *m_editor;
    QString m_findText;
    QString m_replaceText;
    bool m_caseSensitive;
    bool m_wholeWord;
    bool m_wrapAround;
    bool m_backwards;
    int m_highlightIndicator;
    QList<SearchResult> m_searchResults;
    int m_savedFirstVisibleLine;
    QString m_initialFindText;
};

#endif // FINDREPLACEMANAGER_H
