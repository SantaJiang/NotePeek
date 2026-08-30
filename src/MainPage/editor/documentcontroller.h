#ifndef DOCUMENTCONTROLLER_H
#define DOCUMENTCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>

#include <memory>

#include "editor/documentpersistenceservice.h"

class CustomEditor;
class DocumentPage;
class DocumentSaveCoordinator;
class DocumentSaveTransactionStore;
class DocumentEncodingReloadService;
class EditorManager;
class EditorContentReplacer;
class ExternalFileWatcher;
class FindReplaceManager;
class EditorPane;
class MarkdownPreviewer;
struct DocumentEncodingReloadResult;

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

    EditorPane *pane() const;
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
    bool saveEditor(CustomEditor *editor);
    // Saves every modified document owned by this pane. The optional set lets
    // callers deduplicate Scintilla documents shared by split views.
    bool saveAll(QSet<void *> *processedDocuments = nullptr);
    bool maybeSave(CustomEditor *editor);
    bool saveEditorForClose(CustomEditor *editor);
    bool saveFile(CustomEditor *editor, const QString &filePath);
    bool hasPendingSaves() const;
    void waitForPendingSaves();
    void requestClose(int index);
    bool closeFileIfUnmodified(const QString &filePath);
    void createDefaultDocument();
    int indexOfFile(const QString &filePath) const;
    bool isEditorLoading(CustomEditor *editor) const;

    // Shared documents (split view)
    void openSharedDocument(CustomEditor *sourceEditor, bool activate = true);
    bool adoptSharedDocumentPath(CustomEditor *sourceEditor, const QString &newFilePath);

    // Encoding overrides. Reopen reinterprets the on-disk bytes in the given
    // encoding (prompting if unsaved changes would be discarded); convert
    // changes the target format and saves the document in it.
    void reopenCurrentWithEncoding(DocumentEncoding encoding);
    bool convertEncodingAndSave(DocumentEncoding encoding);

    // External change handling. Disk IO for external reloads happens in
    // SplitEditorCoordinator (off the UI thread); the controller only applies
    // already-loaded content to its editors.
    bool hasTrackedFile(const QString &filePath) const;
    bool applyReloadedContent(const QString &filePath,
                              const QByteArray &utf8Content,
                              const DocumentFormat &format,
                              bool performanceDegraded = false,
                              int detectedLanguage = 0);
    bool hasModifiedEditorForFile(const QString &filePath) const;
    void markExternalConflict(const QString &filePath, bool deleted);

signals:
    void currentEditorChanged(CustomEditor *editor);
    void editorModificationChanged(CustomEditor *editor, bool modified);
    void editorCreated(CustomEditor *editor);
    void editorClosed(CustomEditor *editor);
    void fileOpened(CustomEditor *editor, const QString &filePath);
    void emojiFontInstallationRecommended(const QString &filePath);
    void fileOpenFailed(const QString &filePath);
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
    void onFileReloadedWithEncoding(const DocumentEncodingReloadResult &result);

private:
    struct PendingLoad
    {
        QPointer<CustomEditor> editor;
        QPointer<DocumentPage> page;
        QString filePath;
        bool activate = true;
    };

    struct PendingEncodingReload
    {
        QPointer<CustomEditor> editor;
        QString filePath;
        quint64 expectedRevision = 0;
        quint64 reloadGeneration = 0;
        bool wasReadOnly = false;
    };

    void connectEditorSignals(CustomEditor *editor);
    void finalizeLoadedEditor(CustomEditor *editor,
                              DocumentPage *page,
                              const QString &filePath,
                              const DocumentFormat &format,
                              bool largeFile,
                              int detectedLanguage,
                              bool activate,
                              bool recommendEmojiFont,
                              int beforeLines);
    bool closeDefaultEditorIfUnused();
    void dispatchAsyncSave(CustomEditor *editor,
                           const QString &filePath,
                           const QByteArray &utf8Content,
                           const QString &oldFilePath,
                           const QString &newFilePath);
    QString effectiveSavePath(CustomEditor *editor) const;
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
    EditorContentReplacer *m_contentReplacer;
    DocumentPersistenceService *m_persistence;
    DocumentSaveCoordinator *m_saveCoordinator;
    std::unique_ptr<DocumentSaveTransactionStore> m_saveTransactions;
    DocumentEncodingReloadService *m_encodingReloadService;
    ExternalFileWatcher *m_externalFileWatcher;
    CustomEditor *m_defaultEditor;
    bool m_createDefaultWhenEmpty;
    MarkdownPreviewer *m_markdownPreviewer = nullptr;
    quint64 m_nextLoadRequestId = 1;
    quint64 m_nextEncodingReloadRequestId = 1;
    QHash<quint64, PendingLoad> m_pendingLoads;
    QHash<quint64, PendingEncodingReload> m_pendingEncodingReloads;
    QSet<CustomEditor *> m_editorsLoading;
    QHash<CustomEditor *, quint64> m_editorContentRevisions;
    // True while a synchronous save drain is processing events; guards
    // against re-entrant document mutations during the wait.
    bool m_drainingSaves = false;
};

#endif // DOCUMENTCONTROLLER_H
