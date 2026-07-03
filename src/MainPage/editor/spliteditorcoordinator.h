#ifndef SPLITEDITORCOORDINATOR_H
#define SPLITEDITORCOORDINATOR_H

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

class CustomEditor;
class DocumentController;
class QSplitter;
class QWidget;

// Keeps the two editor panes of the split view in sync: which pane is
// active, mirroring opened/closed/renamed files and routing pane-level
// signals to the main window.
class SplitEditorCoordinator : public QObject
{
    Q_OBJECT

public:
    SplitEditorCoordinator(DocumentController *leftController,
                           DocumentController *rightController,
                           QSplitter *splitter,
                           bool singleFileWindow,
                           QObject *parent = nullptr);

    DocumentController *activeController() const;
    QList<DocumentController *> controllers() const;
    void setActiveController(DocumentController *controller);
    void setSplitViewEnabled(bool enabled);
    bool closeAllEditors();
    void reapplyThemeToEditorPanes();
    void refreshVisibleMarkdownPreviews();
    void updateActivePaneForFocus(QWidget *widget);

signals:
    void activeControllerChanged(DocumentController *controller);
    void paneModificationChanged(DocumentController *controller, bool modified);
    void paneCopyAvailableChanged(DocumentController *controller, bool available);
    void filesDropped(DocumentController *controller, const QList<QUrl> &urls);

private:
    void connectController(DocumentController *controller);
    DocumentController *otherController(DocumentController *controller) const;
    DocumentController *visibleSingleController() const;
    void setSingleVisibleController(DocumentController *controller);
    void collapseSplitPane(DocumentController *controller);
    void syncOpenedFileToOtherPanes(DocumentController *source, const QString &filePath);
    void syncClosedFileToOtherPanes(DocumentController *source, const QString &filePath);
    void syncRenamedFileToOtherPanes(DocumentController *source,
                                     CustomEditor *sourceEditor,
                                     const QString &oldFilePath,
                                     const QString &newFilePath);
    void reloadExternallyChangedFile(DocumentController *source, const QString &filePath);
    void showSameFileInOtherPane(DocumentController *source);

    DocumentController *m_leftController;
    DocumentController *m_rightController;
    QSplitter *m_splitter;
    DocumentController *m_activeController;
    bool m_syncingEditorPanes;
    bool m_splitViewEnabled;
    bool m_singleFileWindow;
};

#endif // SPLITEDITORCOORDINATOR_H
