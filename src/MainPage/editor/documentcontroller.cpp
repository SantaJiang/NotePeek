#include "editor/documentcontroller.h"

#include "apppreferences.h"
#include "decorators/highlightedscrollbardecorator.h"
#include "editor/customeditor.h"
#include "editor/documentio.h"
#include "editor/documentpage.h"
#include "editor/documentpersistenceservice.h"
#include "editor/documentregistry.h"
#include "editor/editormanager.h"
#include "editor/externalfilewatcher.h"
#include "editor/filesavedialog.h"
#include "editor/lexerfactory.h"
#include "editor/editorpane.h"
#include "find/findreplacemanager.h"

#include <QApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QSet>
#include <QSignalBlocker>

namespace {

bool isMarkdownPath(const QString &filePath)
{
    return LexerFactory::languageFromFilePath(filePath) == LexerFactory::MARKDOWN;
}

QHash<void *, quint64> &latestDocumentSaveGenerations()
{
    static QHash<void *, quint64> generations;
    return generations;
}

quint64 nextDocumentSaveGeneration(void *documentKey)
{
    static quint64 nextGeneration = 1;
    const quint64 generation = nextGeneration++;
    latestDocumentSaveGenerations().insert(documentKey, generation);
    return generation;
}

bool isLatestDocumentSave(void *documentKey, quint64 generation)
{
    return documentKey
           && latestDocumentSaveGenerations().value(documentKey) == generation;
}

bool hasOutstandingDocumentSave(CustomEditor *editor)
{
    if (!editor)
    {
        return false;
    }
    void *documentKey = editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
    if (!documentKey)
    {
        documentKey = editor;
    }
    return latestDocumentSaveGenerations().contains(documentKey);
}

void finishDocumentSave(void *documentKey, quint64 generation)
{
    if (isLatestDocumentSave(documentKey, generation))
    {
        latestDocumentSaveGenerations().remove(documentKey);
    }
}

} // namespace

DocumentController::DocumentController(EditorPane *pane,
                                       bool createDefaultWhenEmpty,
                                       QObject *parent)
    : QObject(parent)
    , m_pane(pane)
    , m_editorManager(new EditorManager(this))
    , m_persistence(new DocumentPersistenceService(this))
    , m_externalFileWatcher(new ExternalFileWatcher(this))
    , m_defaultEditor(nullptr)
    , m_createDefaultWhenEmpty(createDefaultWhenEmpty)
{
    connect(m_editorManager, &EditorManager::filesDropped,
            this, &DocumentController::filesDropped);
    connect(m_externalFileWatcher, &ExternalFileWatcher::externalFileChanged,
            this, &DocumentController::externalFileChanged);
    connect(m_persistence, &DocumentPersistenceService::fileLoaded,
            this, &DocumentController::onFileLoaded);
    connect(m_persistence, &DocumentPersistenceService::fileSaved,
            this, &DocumentController::onFileSaved);
    connect(m_pane, &EditorPane::pageCloseRequested,
            this, &DocumentController::requestClose);
    connect(m_pane, &EditorPane::currentPageChanged,
            this, &DocumentController::onCurrentPageChanged);

    if (m_createDefaultWhenEmpty)
    {
        createDefaultDocument();
    }
}

DocumentController::~DocumentController()
{
    // Keep the receiver and document-generation bookkeeping alive until every
    // snapshot submitted by this controller has produced a terminal result.
    waitForPendingSaves();
}

CustomEditor *DocumentController::currentEditor() const
{
    return m_pane->currentEditor();
}

void DocumentController::setCreateDefaultWhenEmpty(bool createDefault)
{
    m_createDefaultWhenEmpty = createDefault;

    if (!createDefault && m_defaultEditor)
    {
        closeDefaultEditorIfUnused();
    }
    else if (createDefault && !m_defaultEditor && m_pane->count() == 0)
    {
        createDefaultDocument();
    }
}

void DocumentController::attachFindReplaceManager(FindReplaceManager *manager)
{
    CustomEditor *editor = currentEditor();
    if (!editor || !manager)
        return;

    manager->setCurrentEditor(editor);

    auto *decorator = editor->findChild<HighlightedScrollBarDecorator *>();
    if (decorator)
    {
        connect(manager, &FindReplaceManager::searchResultsChanged,
                decorator, &HighlightedScrollBarDecorator::updateScrollBar,
                Qt::UniqueConnection);
    }
}

void DocumentController::openFile(const QString &filePath, bool activate)
{
    const int previousIndex = m_pane->currentIndex();

    // If the file is already open, just activate it.
    CustomEditor *existingEditor = m_editorManager->getEditorByFilePath(filePath);
    if (existingEditor)
    {
        closeDefaultEditorIfUnused();
        const int index = m_pane->indexOfEditor(existingEditor);
        if (index != -1 && activate)
        {
            m_pane->setCurrentIndex(index);
        }
        return;
    }

    CustomEditor *editor = m_editorManager->createEditorForFile(filePath, false);
    if (!editor)
    {
        return;
    }
    editor->setFilePath(filePath);
    editor->setReadOnly(true);

    const QFileInfo fileInfo(filePath);
    DocumentPage *page = createPage(editor, filePath);
    const int tabIndex = m_pane->addPage(page, fileInfo.fileName(), filePath);
    m_pane->setCurrentIndex(tabIndex);
    connectEditorSignals(editor);

    const quint64 requestId = m_nextAsyncRequestId++;
    m_pendingLoads.insert(requestId, { editor, page, filePath, previousIndex, activate });
    m_persistence->loadFileAsync({ requestId, filePath });
}

void DocumentController::newDocument()
{
    if (maybeSave(currentEditor()))
    {
        createDefaultDocument();
    }
}

bool DocumentController::save()
{
    CustomEditor *editor = currentEditor();
    if (!editor)
        return false;

    const QString filePath = editor->filePath();
    return filePath.isEmpty() ? saveAs() : saveFile(editor, filePath);
}

bool DocumentController::saveAs()
{
    CustomEditor *editor = currentEditor();
    if (!editor)
        return false;

    const QString fileName = m_persistence->promptSaveFileName(m_pane);
    if (fileName.isEmpty())
        return false;

    return saveFile(editor, fileName);
}

bool DocumentController::maybeSave(CustomEditor *editor)
{
    if (!editor || !editor->isModified())
        return true;

    m_pane->focusEditor(editor);

    QMap<CustomEditor *, QString> files;
    files.insert(editor, m_persistence->displayNameForSavePrompt(editor));

    FileSaveDialog dialog(files, m_pane);
    const int result = dialog.exec();

    if (result == QDialog::Accepted)
    {
        const QString filePath = editor->filePath();
        if (filePath.isEmpty())
        {
            const QString fileName = m_persistence->promptSaveFileName(m_pane);
            return !fileName.isEmpty() && saveFileSynchronously(editor, fileName);
        }
        return saveFileSynchronously(editor, filePath);
    }
    if (result == FileSaveDialog::DontSave)
    {
        return true;
    }

    return false;
}

bool DocumentController::saveEditorForClose(CustomEditor *editor)
{
    if (!editor)
    {
        return false;
    }

    QString filePath = editor->filePath();
    if (filePath.isEmpty())
    {
        filePath = m_persistence->promptSaveFileName(m_pane->parentWidget());
        if (filePath.isEmpty())
        {
            return false;
        }
    }

    return saveFileSynchronously(editor, filePath);
}

bool DocumentController::saveFile(CustomEditor *editor, const QString &filePath)
{
    if (!editor)
        return false;

    const QString oldFilePath = editor->filePath().isEmpty()
                                    ? QString()
                                    : QFileInfo(editor->filePath()).absoluteFilePath();
    const QString newFilePath = QFileInfo(filePath).absoluteFilePath();

    QString errorMessage;
    QByteArray utf8Content;
    if (!m_persistence->saveEditorSnapshot(editor, filePath, &utf8Content, &errorMessage))
    {
        QMessageBox::warning(m_pane, tr("NotePeek"),
                             tr("Cannot prepare file %1 for saving:\n%2.")
                             .arg(filePath)
                             .arg(errorMessage));
        return false;
    }

    // DocumentPersistenceService owns a process-wide FIFO per destination path,
    // so saves from either split pane are serialized as well.
    dispatchAsyncSave(editor, filePath, utf8Content, oldFilePath, newFilePath);

    editor->setModified(false);
    emit editorModificationChanged(editor, false);
    if (editor == currentEditor())
    {
        emit modificationChanged(false);
    }

    setSavingTitle(editor, filePath);
    return true;
}

void DocumentController::dispatchAsyncSave(CustomEditor *editor,
                                           const QString &filePath,
                                           const QByteArray &utf8Content,
                                           const QString &oldFilePath,
                                           const QString &newFilePath)
{
    const quint64 requestId = m_nextAsyncRequestId++;
    void *documentKey = editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
    if (!documentKey)
    {
        documentKey = editor;
    }
    const quint64 documentGeneration = nextDocumentSaveGeneration(documentKey);
    m_pendingSaves.insert(requestId, { editor,
                                       oldFilePath,
                                       newFilePath,
                                       utf8Content,
                                       documentKey,
                                       documentGeneration });
    m_persistence->saveFileAsync({ requestId,
                                   filePath,
                                   utf8Content,
                                   reinterpret_cast<quintptr>(documentKey) });
}

void DocumentController::setSavingTitle(CustomEditor *editor, const QString &filePath)
{
    const int index = m_pane->indexOfEditor(editor);
    if (index != -1)
    {
        QString title = QFileInfo(filePath).fileName();
        if (title.isEmpty())
        {
            title = tr("untitled.txt");
        }
        m_pane->setPageTitle(index, title + tr(" (saving)"));
    }
}

void DocumentController::onFileLoaded(const DocumentLoadResult &result)
{
    const PendingLoad pending = m_pendingLoads.take(result.requestId);
    if (!pending.editor || !pending.page || pending.filePath != result.filePath)
    {
        return;
    }

    CustomEditor *editor = pending.editor.data();
    DocumentPage *page = pending.page.data();
    const int tabIndex = m_pane->indexOfEditor(editor);
    if (tabIndex == -1)
    {
        return;
    }

    editor->setReadOnly(false);

    if (!result.success)
    {
        m_pane->removePage(tabIndex);
        emit editorClosed(editor);
        m_editorManager->forgetEditor(editor);
        page->deleteLater();

        if (m_createDefaultWhenEmpty && m_pane->count() == 0)
        {
            createDefaultDocument();
        }

        QMessageBox::warning(m_pane, tr("NotePeek"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(result.filePath)
                             .arg(result.errorMessage));
        return;
    }

    editor->setUpdatesEnabled(false);
    editor->setText(result.content);
    editor->setUpdatesEnabled(true);

    if (!result.largeFile && AppPreferences::instance().markdownDefaultPreview() && m_markdownPreviewer)
    {
        if (auto *markdownPage = qobject_cast<MarkdownDocumentPage *>(page))
        {
            markdownPage->setInitialViewMode(true, m_markdownPreviewer, pending.activate);
        }
    }

    setCurrentFile(editor, result.filePath);
    m_editorManager->updateLineNumberMarginWidth(editor);
    closeDefaultEditorIfUnused();
    if (pending.activate)
    {
        m_pane->focusEditor(editor);
    }
    else if (pending.previousIndex >= 0 && pending.previousIndex < m_pane->count())
    {
        m_pane->setCurrentIndex(pending.previousIndex);
    }

    emit fileOpened(editor, result.filePath);
}

void DocumentController::onFileSaved(const DocumentSaveResult &result)
{
    const PendingSave pending = m_pendingSaves.take(result.requestId);
    if (pending.newFilePath.isEmpty())
    {
        return;
    }

    CustomEditor *editor = pending.editor.data();
    const bool latestForEditor = editor
                                 && isLatestDocumentSave(pending.documentKey,
                                                         pending.documentGeneration);
    finishDocumentSave(pending.documentKey, pending.documentGeneration);

    if (result.superseded)
    {
        // A newer snapshot for this shared document/path remains queued. This
        // request never touched disk and must not affect editor or tab state.
        return;
    }

    if (!result.success)
    {
        // An obsolete failure must not dirty a document after a newer request
        // has already superseded it.
        if (latestForEditor && !editor->isModified())
        {
            editor->setModified(true);
            emit editorModificationChanged(editor, true);
            if (editor == currentEditor())
            {
                emit modificationChanged(true);
            }
        }

        // Restore the tab title from the transient "(saving)" state so the tab
        // does not stay stuck showing that a save is in progress.
        if (latestForEditor)
        {
            const int index = m_pane->indexOfEditor(editor);
            if (index != -1)
            {
                const QString shownName = QFileInfo(result.filePath).fileName();
                m_pane->setPageTitle(index,
                                     shownName + (editor->isModified() ? QStringLiteral("*") : QString()));
            }
        }

        if (latestForEditor)
        {
            QMessageBox::warning(m_pane, tr("NotePeek"),
                                 tr("Cannot write file %1:\n%2.")
                                 .arg(result.filePath)
                                 .arg(result.errorMessage));
        }
        return;
    }

    if (latestForEditor)
    {
        QByteArray currentContent;
        QString snapshotError;
        if (m_persistence->saveEditorSnapshot(editor,
                                              result.filePath,
                                              &currentContent,
                                              &snapshotError)
            && currentContent == pending.utf8Content)
        {
            editor->setModified(false);
        }
        applySavedFileState(editor, result.filePath, pending.oldFilePath, pending.newFilePath);
    }
}

bool DocumentController::saveFileSynchronously(CustomEditor *editor, const QString &filePath)
{
    if (!editor)
    {
        return false;
    }

    const QString oldFilePath = editor->filePath().isEmpty()
                                    ? QString()
                                    : QFileInfo(editor->filePath()).absoluteFilePath();
    const QString newFilePath = QFileInfo(filePath).absoluteFilePath();

    // Supersede every older async result for this shared Scintilla document,
    // including requests submitted by the other split pane to another path.
    void *documentKey = editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
    if (!documentKey)
    {
        documentKey = editor;
    }
    quint64 documentGeneration = nextDocumentSaveGeneration(documentKey);

    // A synchronous save is the final authority for this document. Drain older
    // requests for the editor (including Save As to another path), then drain
    // requests from other panes/windows that target this destination.
    waitForPendingSaves(editor);
    m_persistence->waitForPendingSavePath(newFilePath);
    if (!isLatestDocumentSave(documentKey, documentGeneration))
    {
        documentGeneration = nextDocumentSaveGeneration(documentKey);
    }

    QString errorMessage;
    QByteArray utf8Content;
    if (!m_persistence->saveEditorSnapshot(editor, filePath, &utf8Content, &errorMessage))
    {
        finishDocumentSave(documentKey, documentGeneration);
        QMessageBox::warning(m_pane, tr("NotePeek"),
                             tr("Cannot prepare file %1 for saving:\n%2.")
                             .arg(filePath)
                             .arg(errorMessage));
        return false;
    }

    if (!DocumentIO::saveUtf8File(filePath, utf8Content, &errorMessage))
    {
        finishDocumentSave(documentKey, documentGeneration);
        QMessageBox::warning(m_pane, tr("NotePeek"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(filePath)
                             .arg(errorMessage));
        return false;
    }

    editor->setModified(false);
    applySavedFileState(editor, filePath, oldFilePath, newFilePath);
    finishDocumentSave(documentKey, documentGeneration);
    return true;
}

void DocumentController::applySavedFileState(CustomEditor *editor,
                                             const QString &filePath,
                                             const QString &oldFilePath,
                                             const QString &newFilePath)
{
    if (!editor)
    {
        return;
    }

    recordFileState(newFilePath);
    editor->setFilePath(filePath);
    m_editorManager->updateEditorSyntax(editor, filePath);

    const QString shownName = QFileInfo(filePath).fileName();
    const int index = m_pane->indexOfEditor(editor);
    if (index != -1)
    {
        m_pane->setPageTitle(index, shownName + (editor->isModified() ? QStringLiteral("*") : QString()));
        m_pane->setPageToolTip(index, filePath);
    }

    if (newFilePath != oldFilePath)
    {
        refreshPageTypeForFile(editor, filePath);
        emit filePathChanged(editor, oldFilePath, newFilePath);
    }

    emit editorModificationChanged(editor, editor->isModified());
    if (editor == currentEditor())
    {
        emit modificationChanged(editor->isModified());
        emit currentFileChanged(editor, filePath);
    }
    refreshWatchedFiles();
}

void DocumentController::waitForPendingSaves()
{
    while (!m_pendingSaves.isEmpty())
    {
        QEventLoop loop;
        connect(m_persistence, &DocumentPersistenceService::fileSaved,
                &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
}

bool DocumentController::hasPendingSave(CustomEditor *editor) const
{
    if (!editor)
    {
        return false;
    }

    for (auto it = m_pendingSaves.cbegin(); it != m_pendingSaves.cend(); ++it)
    {
        if (it.value().editor == editor)
        {
            return true;
        }
    }
    return false;
}

void DocumentController::waitForPendingSaves(CustomEditor *editor)
{
    while (hasPendingSave(editor))
    {
        QEventLoop loop;
        connect(m_persistence, &DocumentPersistenceService::fileSaved,
                &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
}

void DocumentController::requestClose(int index)
{
    DocumentPage *page = m_pane->pageAt(index);
    CustomEditor *editor = page ? page->editor() : nullptr;
    if (!editor)
        return;

    // saveFile() clears the modified flag while its snapshot is in flight.
    // Wait before deciding whether the document can be closed so failures can
    // restore the dirty state and be handled by maybeSave().
    waitForPendingSaves(editor);

    if (maybeSave(editor))
    {
        if (editor == m_defaultEditor)
        {
            m_defaultEditor = nullptr;
        }
        m_pane->removePage(index);
        emit editorClosed(editor);

        m_editorManager->forgetEditor(editor);
        page->deleteLater();
        refreshWatchedFiles();

        if (m_createDefaultWhenEmpty && m_pane->count() == 0)
        {
            createDefaultDocument();
        }
    }
}

bool DocumentController::closeFileIfUnmodified(const QString &filePath)
{
    const int index = indexOfFile(filePath);
    if (index == -1)
    {
        return true;
    }

    DocumentPage *page = m_pane->pageAt(index);
    CustomEditor *editor = page ? page->editor() : nullptr;
    if (!editor)
    {
        return false;
    }

    waitForPendingSaves(editor);
    if (editor->isModified())
    {
        return false;
    }

    if (editor == m_defaultEditor)
    {
        m_defaultEditor = nullptr;
    }

    m_pane->removePage(index);
    emit editorClosed(editor);
    m_editorManager->forgetEditor(editor);
    page->deleteLater();
    refreshWatchedFiles();

    if (m_createDefaultWhenEmpty && m_pane->count() == 0)
    {
        createDefaultDocument();
    }

    return true;
}

void DocumentController::createDefaultDocument()
{
    CustomEditor *editor = m_editorManager->createEditor(tr("Untitled"));
    DocumentPage *page = createPage(editor, QString());
    const int tabIndex = m_pane->addPage(page, tr("Untitled"), QString());
    m_pane->setCurrentIndex(tabIndex);

    m_defaultEditor = editor;

    connectEditorSignals(editor);

    emit editorCreated(editor);
}

int DocumentController::indexOfFile(const QString &filePath) const
{
    CustomEditor *editor = m_editorManager->getEditorByFilePath(filePath);
    return editor ? m_pane->indexOfEditor(editor) : -1;
}

void DocumentController::openSharedDocument(CustomEditor *sourceEditor, bool activate)
{
    if (!sourceEditor)
    {
        return;
    }

    const int existingIndex = findSharedDocumentIndex(sourceEditor);
    if (existingIndex != -1)
    {
        if (activate)
        {
            m_pane->setCurrentIndex(existingIndex);
        }
        return;
    }

    const QString filePath = sourceEditor->filePath();
    if (!filePath.isEmpty())
    {
        closeFileIfUnmodified(filePath);
    }

    QString displayName = filePath.isEmpty() ? tr("Untitled") : QFileInfo(filePath).fileName();
    if (sourceEditor->isModified())
    {
        displayName += '*';
    }

    CustomEditor *editor = m_editorManager->createEditorSharingDocument(sourceEditor, displayName);
    if (!editor)
    {
        return;
    }

    DocumentPage *page = createPage(editor, filePath);
    connectEditorSignals(editor);
    const int tabIndex = m_pane->addPage(page, displayName, filePath);

    if (filePath.isEmpty() && !sourceEditor->isModified() && sourceEditor->text().isEmpty())
    {
        m_defaultEditor = editor;
    }

    if (activate)
    {
        m_pane->setCurrentIndex(tabIndex);
    }

    emit editorCreated(editor);
    emit currentEditorChanged(editor);
    emit modificationChanged(editor->isModified());
    recordFileState(filePath);
    refreshWatchedFiles();
}

bool DocumentController::adoptSharedDocumentPath(CustomEditor *sourceEditor, const QString &newFilePath)
{
    if (!sourceEditor || newFilePath.isEmpty())
    {
        return false;
    }

    const int index = findSharedDocumentIndex(sourceEditor);
    if (index == -1)
    {
        return false;
    }

    CustomEditor *editor = m_pane->editorAt(index);
    if (!editor)
    {
        return false;
    }

    setCurrentFile(editor, newFilePath);
    m_editorManager->updateEditorSyntax(editor, newFilePath);
    refreshPageTypeForFile(editor, newFilePath);
    return true;
}

bool DocumentController::reloadFileFromDiskIfUnmodified(const QString &filePath)
{
    const QString normalizedPath = DocumentRegistry::normalizedFilePath(filePath);
    if (normalizedPath.isEmpty()
        || !DocumentRegistry::trackedFilePaths(m_editorManager->getEditors()).contains(normalizedPath))
    {
        return false;
    }

    const QFileInfo fileInfo(normalizedPath);
    const bool exists = fileInfo.exists() && fileInfo.isFile();

    if (!exists)
    {
        recordFileState(normalizedPath);
        refreshWatchedFiles();
        return false;
    }

    if (hasModifiedEditorForFile(normalizedPath))
    {
        return false;
    }

    QString content;
    QString errorMessage;
    if (!m_persistence->loadFile(normalizedPath, &content, &errorMessage))
    {
        refreshWatchedFiles();
        return false;
    }

    recordFileState(normalizedPath);

    bool reloaded = false;
    bool reloadedCurrentEditor = false;
    QSet<void *> reloadedDocuments;
    for (CustomEditor *editor : m_editorManager->getEditors())
    {
        if (!editor || DocumentRegistry::normalizedFilePath(editor->filePath()) != normalizedPath)
        {
            continue;
        }

        // Editors in split view can share one Scintilla document; reload it once.
        void *document = editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
        if (document && reloadedDocuments.contains(document))
        {
            continue;
        }
        if (document)
        {
            reloadedDocuments.insert(document);
        }

        int line = 0;
        int index = 0;
        editor->getCursorPosition(&line, &index);
        const int firstVisibleLine = editor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
        const int xOffset = editor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET);

        const QSignalBlocker blocker(editor);
        editor->setText(content);
        editor->setModified(false);

        const int targetLine = qBound(0, line, qMax(0, editor->lines() - 1));
        const int targetIndex = qBound(0, index, editor->lineLength(targetLine));
        editor->setCursorPosition(targetLine, targetIndex);
        editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
        editor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);
        m_editorManager->updateLineNumberMarginWidth(editor);

        if (DocumentPage *page = m_pane->pageForEditor(editor))
        {
            page->refreshPreview();
        }

        emit editorModificationChanged(editor, false);
        if (editor == currentEditor())
        {
            reloadedCurrentEditor = true;
        }
        reloaded = true;
    }

    if (reloadedCurrentEditor)
    {
        emit modificationChanged(false);
    }

    refreshWatchedFiles();
    return reloaded;
}

bool DocumentController::hasModifiedEditorForFile(const QString &filePath) const
{
    return DocumentRegistry::hasModifiedEditorForFile(m_editorManager->getEditors(), filePath);
}

void DocumentController::onCurrentPageChanged(int index)
{
    if (index == -1)
        return;

    DocumentPage *page = m_pane->pageAt(index);
    CustomEditor *editor = page ? page->editor() : nullptr;
    if (!editor)
        return;

    emit currentEditorChanged(editor);

    // The trailing '*' on the page title marks unsaved changes.
    emit modificationChanged(m_pane->pageTitle(index).endsWith('*'));

    emit copyAvailable(page->hasCopySelection());
}

void DocumentController::onEditorModificationChanged()
{
    CustomEditor *editor = qobject_cast<CustomEditor *>(sender());
    if (!editor)
        return;

    // A default document that has been edited is no longer disposable.
    if (editor == m_defaultEditor && editor->isModified())
    {
        m_defaultEditor = nullptr;
    }

    const int index = m_pane->indexOfEditor(editor);
    if (index == -1)
        return;

    QString title = m_pane->pageTitle(index);
    if (title.endsWith('*'))
    {
        title.chop(1);
    }

    if (editor->isModified())
    {
        m_pane->setPageTitle(index, title + '*');
        emit modificationChanged(true);
    }
    else
    {
        m_pane->setPageTitle(index, title);
        emit modificationChanged(false);
    }

    emit editorModificationChanged(editor, editor->isModified());

    if (!editor->isModified()
        && !editor->filePath().isEmpty()
        && !hasOutstandingDocumentSave(editor))
    {
        m_externalFileWatcher->requestExternalChangeCheck(editor->filePath());
    }
}

void DocumentController::connectEditorSignals(CustomEditor *editor)
{
    if (!editor)
        return;

    connect(editor, SIGNAL(modificationChanged(bool)), this, SLOT(onEditorModificationChanged()));
    connect(editor, SIGNAL(copyAvailable(bool)), this, SIGNAL(copyAvailable(bool)));
}

bool DocumentController::closeDefaultEditorIfUnused()
{
    if (!m_defaultEditor || m_defaultEditor->isModified() || !m_defaultEditor->text().isEmpty())
    {
        return false;
    }

    const int index = m_pane->indexOfEditor(m_defaultEditor);
    if (index == -1)
    {
        m_defaultEditor = nullptr;
        return false;
    }

    CustomEditor *editor = m_defaultEditor;
    DocumentPage *page = m_pane->pageForEditor(editor);
    m_pane->removePage(index);
    emit editorClosed(editor);
    m_editorManager->forgetEditor(editor);

    if (page)
    {
        page->deleteLater();
    }
    else
    {
        editor->deleteLater();
    }

    m_defaultEditor = nullptr;
    refreshWatchedFiles();
    return true;
}

DocumentPage *DocumentController::createPage(CustomEditor *editor, const QString &filePath)
{
    DocumentPage *page = isMarkdownPath(filePath)
                             ? static_cast<DocumentPage *>(new MarkdownDocumentPage(editor, m_pane))
                             : static_cast<DocumentPage *>(new PlainDocumentPage(editor, m_pane));
    connect(page, &DocumentPage::copyAvailableChanged, this, &DocumentController::copyAvailable);
    return page;
}

void DocumentController::refreshPageTypeForFile(CustomEditor *editor, const QString &filePath)
{
    DocumentPage *oldPage = m_pane->pageForEditor(editor);
    if (!oldPage)
    {
        return;
    }

    const bool needsMarkdownPage = isMarkdownPath(filePath);
    const bool hasMarkdownPage = qobject_cast<MarkdownDocumentPage *>(oldPage) != nullptr;
    if (needsMarkdownPage == hasMarkdownPage)
    {
        oldPage->hidePreview();
        return;
    }

    const int index = m_pane->indexOfEditor(editor);
    if (index == -1)
    {
        return;
    }

    CustomEditor *ownedEditor = oldPage->takeEditor();
    if (!ownedEditor)
    {
        return;
    }

    DocumentPage *newPage = createPage(ownedEditor, filePath);
    m_pane->replacePage(index, newPage);
    oldPage->deleteLater();

    emit currentEditorChanged(ownedEditor);
    emit copyAvailable(newPage->hasCopySelection());
}

void DocumentController::setCurrentFile(CustomEditor *editor, const QString &filePath)
{
    if (!editor)
        return;

    editor->setFilePath(filePath);
    editor->setModified(false);

    const QString shownName = filePath.isEmpty()
                                  ? QStringLiteral("untitled.txt")
                                  : QFileInfo(filePath).fileName();

    const int index = m_pane->indexOfEditor(editor);
    if (index != -1)
    {
        m_pane->setPageTitle(index, shownName);
        m_pane->setPageToolTip(index, filePath);
    }

    emit editorModificationChanged(editor, false);
    if (editor == currentEditor())
    {
        emit modificationChanged(false);
        emit currentFileChanged(editor, filePath);
    }

    recordFileState(filePath);
    refreshWatchedFiles();
}

int DocumentController::findSharedDocumentIndex(CustomEditor *sourceEditor) const
{
    if (!sourceEditor)
    {
        return -1;
    }

    void *sourceDocument = sourceEditor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
    if (!sourceDocument)
    {
        return -1;
    }

    for (int i = 0; i < m_pane->count(); ++i)
    {
        CustomEditor *editor = m_pane->editorAt(i);
        if (editor && editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER) == sourceDocument)
        {
            return i;
        }
    }

    return -1;
}

void DocumentController::refreshWatchedFiles()
{
    m_externalFileWatcher->setTrackedFilePaths(
        DocumentRegistry::trackedFilePaths(m_editorManager->getEditors()));
}

void DocumentController::recordFileState(const QString &filePath)
{
    m_externalFileWatcher->recordFileState(filePath);
}
