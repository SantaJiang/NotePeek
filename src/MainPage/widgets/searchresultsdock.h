#ifndef SEARCHRESULTSDOCK_H
#define SEARCHRESULTSDOCK_H

#include <QObject>
#include <QString>

#include "find/filesearcher.h"

class QDockWidget;
class QLabel;
class QLineEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

// Bottom dock for project-wide search: a query row (pattern, exclusions, case
// and whole-word toggles), a status line, and a file-grouped result tree.
// The search itself runs in FileSearcher off the UI thread.
class SearchResultsDock : public QObject
{
    Q_OBJECT

public:
    explicit SearchResultsDock(QWidget *parent);

    QDockWidget *dockWidget() const { return m_dock; }
    void setVisible(bool visible);
    // Root directory searches run in; empty disables searching with a hint.
    void setSearchRoot(const QString &rootDirectory);
    void focusSearchInput(const QString &initialText = QString());
    void reapplyTheme();

signals:
    void matchActivated(const QString &filePath, int line, int column, int length);

private slots:
    void startOrCancelSearch();
    void onSearchFinished(const FileSearchOutcome &outcome);
    void onItemActivated(QTreeWidgetItem *item, int column);

private:
    void setSearchingState(bool searching);

    QDockWidget *m_dock;
    QLineEdit *m_patternEdit;
    QLineEdit *m_excludeEdit;
    QToolButton *m_caseButton;
    QToolButton *m_wholeWordButton;
    QToolButton *m_searchButton;
    QWidget *m_titleBar;
    QLabel *m_statusLabel;
    QTreeWidget *m_resultsTree;
    FileSearcher *m_searcher;
    QString m_searchRoot;
};

#endif // SEARCHRESULTSDOCK_H
