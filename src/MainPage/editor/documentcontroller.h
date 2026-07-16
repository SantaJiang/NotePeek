#ifndef DOCUMENTCONTROLLER_H
#define DOCUMENTCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>

#include "editor/documentpersistenceservice.h"

class CustomEditor;
class DocumentPage;
class DocumentSaveCoordinator;
class EditorManager;
class ExternalFileWatcher;
class FindReplaceManager;
class EditorPane;
class MarkdownPreviewer;

// Drives the document lifecycle of one editor pane: open/save/close
// workflows, save prompts, the default blank document policy, shared
// documents for split view and reloading externally changed files.
// The pane itself (EditorPane) stays a pure view.
class DocumentController : public QObject
{
    Q_OBJECT

public:
    explicit DocumentController(EditorPane *pane,
                                bool createDefaultWhenEmpty = true,
                                DocumentSaveCoordinator *saveCoordinator = nullptr,
                                ExternalFileWatcher *externalFileWatcher = nullptr,
                                QObject *parent = nullptr);
    ~DocumentController() override;

    EditorPane *pane() const { return m_pane.data(); }
    EditorManager *editorManager() const { return m_editorManager; }
    CustomEditor *currentEditor() const;
    void setCreateDefaultWhenEmpty(bool createDefault);
    void attachFindReplaceManager(FindReplaceManager *manager);

    // Used to realize the "open Markdown in preview" preference when a file is
    // opened.  Optional: without it, Markdown files simply open in source view.
    void setMarkdownPreviewer(MarkdownPreviewer *previewer) { m_markdownPreviewer = previewer; }

    // File workflow
    void openFile(const QString &filePath, bool activate = true);
    void newDocument();
    bool save();
    bool saveAs();
    bool maybeSave(CustomEditor *editor);
    bool saveEditorForClose(CustomEditor *editor);
    bool saveFile(CustomEditor *editor, const QString &filePath);
    bool hasPendingSaves() const { return !m_pendingSaves.isEmpty(); }
    void waitForPendingSaves();
    void requestClose(int index);
    bool closeFileIfUnmodified(const QString &filePath);
    void createDefaultDocument();
    int indexOfFile(const QString &filePath) const;

    // Shared documents (split view)
    void openSharedDocument(CustomEditor *sourceEditor, bool activate = true);
    bool adoptSharedDocumentPath(CustomEditor *sourceEditor, const QString &newFilePath);

    // External change handling
    bool reloadFileFromDiskIfUnmodified(const QString &filePath);
    bool hasModifiedEditorForFile(const QString &filePath) const;
    void markExternalConflict(const QString &filePath, bool deleted);

signals:
    void currentEditorChanged(CustomEditor *editor);
    void editorModificationChanged(CustomEditor *editor, bool modified);
    void editorCreated(CustomEditor *editor);
    void editorClosed(CustomEditor *editor);
    void fileOpened(CustomEditor *editor, const QString &filePath);
    void filePathChanged(CustomEditor *editor, const QString &oldFilePath, const QString &newFilePath);
    void currentFileChanged(CustomEditor *editor, const QString &filePath);
    void externalFileChanged(const QString &filePath);
    void modificationChanged(bool modified);
    void copyAvailable(bool available);
    void filesDropped(const QList<QUrl> &urls);

private slots:
    void onCurrentPageChanged(int index);
    void onEditorModificationChanged();
    void onFileLoaded(const DocumentLoadResult &result);
    void onFileSaved(const DocumentSaveResult &result);

private:
    struct PendingLoad
    {
        QPointer<CustomEditor> editor;
        QPointer<DocumentPage> page;
        QString filePath;
        int previousIndex = -1;
        bool activate = true;
    };

    struct PendingSave
    {
        QPointer<CustomEditor> editor;
        QString oldFilePath;
        QString newFilePath;
        QByteArray utf8Content;
        void *documentKey = nullptr;
        quint64 documentGeneration = 0;
    };

    void connectEditorSignals(CustomEditor *editor);
    bool closeDefaultEditorIfUnused();
    void dispatchAsyncSave(CustomEditor *editor,
                           const QString &filePath,
                           const QByteArray &utf8Content,
                           const QString &oldFilePath,
                           const QString &newFilePath);
    void setSavingTitle(CustomEditor *editor, const QString &filePath);
    void waitForPendingSaves(CustomEditor *editor);
    bool hasPendingSave(CustomEditor *editor) const;
    bool saveFileSynchronously(CustomEditor *editor, const QString &filePath);
    void applySavedFileState(CustomEditor *editor,
                             const QString &filePath,
                             const QString &oldFilePath,
                             const QString &newFilePath);
    DocumentPage *createPage(CustomEditor *editor, const QString &filePath);
    void refreshPageTypeForFile(CustomEditor *editor, const QString &filePath);
    void setCurrentFile(CustomEditor *editor, const QString &filePath);
    int findSharedDocumentIndex(CustomEditor *sourceEditor) const;
    void refreshWatchedFiles();
    void recordFileState(const QString &filePath);
    bool confirmExternalOverwrite(CustomEditor *editor, const QString &filePath);

    QPointer<EditorPane> m_pane;
    EditorManager *m_editorManager;
    DocumentPersistenceService *m_persistence;
    DocumentSaveCoordinator *m_saveCoordinator;
    ExternalFileWatcher *m_externalFileWatcher;
    CustomEditor *m_defaultEditor;
    bool m_createDefaultWhenEmpty;
    MarkdownPreviewer *m_markdownPreviewer = nullptr;
    quint64 m_nextAsyncRequestId = 1;
    QHash<quint64, PendingLoad> m_pendingLoads;
    QHash<quint64, PendingSave> m_pendingSaves;
};

#endif // DOCUMENTCONTROLLER_H
