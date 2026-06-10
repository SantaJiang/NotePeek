#ifndef DOCUMENTCONTROLLER_H
#define DOCUMENTCONTROLLER_H

#include <QObject>
#include <QString>
#include <QUrl>

class CustomEditor;
class DocumentPage;
class DocumentPersistenceService;
class EditorManager;
class ExternalFileWatcher;
class FindReplaceManager;
class MultiEditor;

// Drives the document lifecycle of one editor pane: open/save/close
// workflows, save prompts, the default blank document policy, shared
// documents for split view and reloading externally changed files.
// The pane itself (MultiEditor) stays a pure view.
class DocumentController : public QObject
{
    Q_OBJECT

public:
    explicit DocumentController(MultiEditor *pane,
                                bool createDefaultWhenEmpty = true,
                                QObject *parent = nullptr);

    MultiEditor *pane() const { return m_pane; }
    EditorManager *editorManager() const { return m_editorManager; }
    CustomEditor *currentEditor() const;
    void setCreateDefaultWhenEmpty(bool createDefault);
    void attachFindReplaceManager(FindReplaceManager *manager);

    // File workflow
    void openFile(const QString &filePath, bool activate = true);
    void newDocument();
    bool save();
    bool saveAs();
    bool maybeSave(CustomEditor *editor);
    bool saveEditorForClose(CustomEditor *editor);
    bool saveFile(CustomEditor *editor, const QString &filePath);
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

private:
    void connectEditorSignals(CustomEditor *editor);
    bool closeDefaultEditorIfUnused();
    DocumentPage *createPage(CustomEditor *editor, const QString &filePath);
    void refreshPageTypeForFile(CustomEditor *editor, const QString &filePath);
    void setCurrentFile(CustomEditor *editor, const QString &filePath);
    int findSharedDocumentIndex(CustomEditor *sourceEditor) const;
    void refreshWatchedFiles();
    void recordFileState(const QString &filePath);

    MultiEditor *m_pane;
    EditorManager *m_editorManager;
    DocumentPersistenceService *m_persistence;
    ExternalFileWatcher *m_externalFileWatcher;
    CustomEditor *m_defaultEditor;
    bool m_createDefaultWhenEmpty;
};

#endif // DOCUMENTCONTROLLER_H
