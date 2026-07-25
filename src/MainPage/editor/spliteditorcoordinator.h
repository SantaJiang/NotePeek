#ifndef SPLITEDITORCOORDINATOR_H
#define SPLITEDITORCOORDINATOR_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

#include "editor/documentformat.h"

class CustomEditor;
class DocumentController;
class ExternalFileWatcher;
class QSplitter;
class QWidget;

// Result of an off-thread disk read for an externally changed file.
struct ExternalReloadResult
{
    bool success = false;
    QByteArray content;
    DocumentFormat format;
    QString errorMessage;
};

// Keeps the editor panes of the split view in sync: which pane is active,
// mirroring opened/closed/renamed files and routing pane-level signals to the
// main window. The coordinator is pane-count agnostic (it operates on an
// ordered controller list); the application currently constructs it with two
// panes.
class SplitEditorCoordinator : public QObject
{
    Q_OBJECT

public:
    SplitEditorCoordinator(const QList<DocumentController *> &controllers,
                           QSplitter *splitter,
                           bool singleFileWindow,
                           ExternalFileWatcher *externalFileWatcher = nullptr,
                           QObject *parent = nullptr);
    // Convenience for the common two-pane setup.
    SplitEditorCoordinator(DocumentController *leftController,
                           DocumentController *rightController,
                           QSplitter *splitter,
                           bool singleFileWindow,
                           ExternalFileWatcher *externalFileWatcher = nullptr,
                           QObject *parent = nullptr);

    DocumentController *activeController() const;
    QList<DocumentController *> controllers() const;
    void setActiveController(DocumentController *controller);
    void setSplitViewEnabled(bool enabled);
    bool closeAllEditors();
    void reapplyThemeToEditorPanes();
    void applyFontPreferencesToEditors();
    void applyCodeFeaturePreferencesToEditors();
    void refreshVisibleMarkdownPreviews();
    void updateActivePaneForFocus(QWidget *widget);

signals:
    void activeControllerChanged(DocumentController *controller);
    void paneModificationChanged(DocumentController *controller, bool modified);
    void paneCopyAvailableChanged(DocumentController *controller, bool available);
    void filesDropped(DocumentController *controller, const QList<QUrl> &urls);
    void externalFileConflict(const QString &filePath, bool deleted);

private:
    void connectController(DocumentController *controller);
    DocumentController *otherController(DocumentController *controller) const;
    DocumentController *visibleSingleController() const;
    void setSingleVisibleController(DocumentController *controller);
    void collapseSplitPane(DocumentController *controller);
    void syncOpenedFileToOtherPanes(DocumentController *source, CustomEditor *sourceEditor, const QString &filePath);
    void syncClosedFileToOtherPanes(DocumentController *source, const QString &filePath);
    void syncRenamedFileToOtherPanes(DocumentController *source,
                                     CustomEditor *sourceEditor,
                                     const QString &oldFilePath,
                                     const QString &newFilePath);
    void reloadExternallyChangedFile(DocumentController *source, const QString &filePath);
    void applyExternalReload(DocumentController *source,
                             const QString &filePath,
                             const ExternalReloadResult &result);
    void showSameFileInOtherPane(DocumentController *source);

    QList<DocumentController *> m_controllers;
    QSplitter *m_splitter;
    DocumentController *m_activeController;
    bool m_syncingEditorPanes;
    bool m_splitViewEnabled;
    bool m_singleFileWindow;
    QSet<QString> m_reloadsInFlight;
    QSet<QString> m_reloadRerunRequested;
};

#endif // SPLITEDITORCOORDINATOR_H
