#include "editor/documentcontroller.h"

#include "apppreferences.h"
#include "decorators/highlightedscrollbardecorator.h"
#include "editor/customeditor.h"
#include "editor/documentio.h"
#include "editor/documentencodingreloadservice.h"
#include "editor/documentfiledialogservice.h"
#include "editor/documentpage.h"
#include "editor/documentpersistenceservice.h"
#include "editor/documentregistry.h"
#include "editor/editorcontentreplacer.h"
#include "editor/editorsnapshotencoder.h"
#include "editor/documentsavecoordinator.h"
#include "editor/documentsavetransactionstore.h"
#include "editor/editormanager.h"
#include "editor/externalfilewatcher.h"
#include "editor/filesavedialog.h"
#include "foundation/documentpath.h"
#include "foundation/performanceconstants.h"
#include <NotePeek/lexerfactory.h>
#include "editor/editorpane.h"
#include "find/findreplacemanager.h"
#include "tool/markdown/emojifontsupport.h"

#include <QAccessible>
#include <QAccessibleEvent>
#include <QApplication>
#include <QEventLoop>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QScopedValueRollback>
#include <QSet>
#include <QSignalBlocker>
#include <QTimer>

#include <algorithm>


namespace {

bool isMarkdownPath(const QString &filePath)
{
    return LexerFactory::languageFromFilePath(filePath) == LexerFactory::MARKDOWN;
}

void applyDocumentLineEnding(CustomEditor *editor, DocumentLineEnding ending)
{
    if (!editor)
        return;
    if (ending == DocumentLineEnding::CrLf)
        editor->setEolMode(CustomEditor::EolWindows);
    else if (ending == DocumentLineEnding::Cr)
        editor->setEolMode(CustomEditor::EolMac);
    else
        editor->setEolMode(CustomEditor::EolUnix);
}

// A lightweight accessibility completion event after a bulk replacement. It
// deliberately carries no text, so it cannot copy a 100 MB document into the
// accessibility bridge.
void notifyAccessibilityDocumentReplaced(CustomEditor *editor, bool reload)
{
    if (!editor)
        return;
#if !defined(QT_NO_ACCESSIBILITY)
    QAccessibleEvent event(editor, reload ? QAccessible::DocumentReload
                                          : QAccessible::DocumentLoadComplete);
    QAccessible::updateAccessibility(&event);
#endif
}

} // namespace

DocumentController::DocumentController(EditorPane *pane,
                                       bool createDefaultWhenEmpty,
                                       DocumentSaveCoordinator *saveCoordinator,
                                       ExternalFileWatcher *externalFileWatcher,
                                       QObject *parent)
    : QObject(parent)
    , m_pane(pane)
    , m_editorManager(new EditorManager(this))
    , m_contentReplacer(new EditorContentReplacer(this))
    , m_persistence(nullptr)
    , m_saveCoordinator(saveCoordinator ? saveCoordinator : new DocumentSaveCoordinator(this))
    , m_saveTransactions(std::make_unique<DocumentSaveTransactionStore>(m_saveCoordinator))
    , m_encodingReloadService(new DocumentEncodingReloadService(this))
    , m_externalFileWatcher(externalFileWatcher ? externalFileWatcher : new ExternalFileWatcher(this))
    , m_defaultEditor(nullptr)
    , m_createDefaultWhenEmpty(createDefaultWhenEmpty)
{
    m_persistence = new DocumentPersistenceService(m_saveCoordinator, this);
    connect(m_editorManager, &EditorManager::filesDropped,
            this, &DocumentController::filesDropped);
    if (!externalFileWatcher)
    {
        connect(m_externalFileWatcher, &ExternalFileWatcher::externalFileChanged,
                this, &DocumentController::externalFileChanged);
    }
    connect(m_persistence, &DocumentPersistenceService::fileLoaded,
            this, &DocumentController::onFileLoaded);
    connect(m_persistence, &DocumentPersistenceService::fileSaved,
            this, &DocumentController::onFileSaved);
    connect(m_encodingReloadService, &DocumentEncodingReloadService::fileReloaded,
            this, &DocumentController::onFileReloadedWithEncoding);
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

EditorPane *DocumentController::pane() const
{
    return m_pane.data();
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
    if (m_drainingSaves)
    {
        return;
    }

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
    m_editorsLoading.insert(editor);

    const QFileInfo fileInfo(filePath);
    DocumentPage *page = createPage(editor, filePath);
    const int tabIndex = m_pane->addPage(page, fileInfo.fileName(), filePath);
    // Adding a background tab must not steal focus from the document the user
    // is reading. QTabWidget selects the first page automatically when the
    // pane was empty, so an explicit selection is only needed for an active
    // open in a non-empty pane.
    if (activate)
        m_pane->setCurrentIndex(tabIndex);
    connectEditorSignals(editor);

    const quint64 requestId = m_nextLoadRequestId++;
    m_pendingLoads.insert(requestId, { editor, page, filePath, activate });
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
    return saveEditor(currentEditor());
}

bool DocumentController::saveAs()
{
    CustomEditor *editor = currentEditor();
    if (!editor)
        return false;

    const QString fileName = DocumentFileDialogService::promptSaveFileName(m_pane);
    if (fileName.isEmpty())
        return false;

    return saveFile(editor, fileName);
}

bool DocumentController::saveEditor(CustomEditor *editor)
{
    if (!editor || m_drainingSaves || isEditorLoading(editor))
        return false;

    QString filePath = effectiveSavePath(editor);
    if (filePath.isEmpty())
    {
        m_pane->focusEditor(editor);
        filePath = DocumentFileDialogService::promptSaveFileName(m_pane);
        if (filePath.isEmpty())
            return false;
    }
    return saveFile(editor, filePath);
}

bool DocumentController::saveAll(QSet<void *> *processedDocuments)
{
    if (m_drainingSaves)
        return false;

    for (CustomEditor *editor : m_editorManager->getEditors())
    {
        if (!editor || !editor->isModified() || isEditorLoading(editor))
            continue;

        void *document = editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
        if (!document)
            document = editor;
        if (processedDocuments && processedDocuments->contains(document))
            continue;

        if (!saveEditor(editor))
            return false;
        if (processedDocuments)
            processedDocuments->insert(document);
    }
    return true;
}

bool DocumentController::maybeSave(CustomEditor *editor)
{
    if (!editor || !editor->isModified())
        return true;

    m_pane->focusEditor(editor);

    QMap<CustomEditor *, QString> files;
    files.insert(editor, DocumentFileDialogService::displayNameForSavePrompt(editor->filePath()));

    FileSaveDialog dialog(files, m_pane);
    const int result = dialog.exec();

    if (result == QDialog::Accepted)
    {
        const QString filePath = effectiveSavePath(editor);
        if (filePath.isEmpty())
        {
            const QString fileName = DocumentFileDialogService::promptSaveFileName(m_pane);
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

    QString filePath = effectiveSavePath(editor);
    if (filePath.isEmpty())
    {
        filePath = DocumentFileDialogService::promptSaveFileName(m_pane->parentWidget());
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
    if (!confirmExternalOverwrite(editor, filePath))
        return false;

    const QString oldFilePath = editor->filePath().isEmpty()
                                    ? QString()
                                    : QFileInfo(editor->filePath()).absoluteFilePath();
    const QString newFilePath = QFileInfo(filePath).absoluteFilePath();

    QString errorMessage;
    QByteArray utf8Content;
    if (!EditorSnapshotEncoder::encode(editor, &utf8Content, &errorMessage))
    {
        QMessageBox::warning(m_pane, tr("NotePeek"),
                             tr("Cannot prepare file %1 for saving:\n%2.")
                             .arg(filePath)
                             .arg(errorMessage));
        return false;
    }

    // The application-scoped save coordinator serializes writes to the same
    // destination even when they originate from different split panes.
    dispatchAsyncSave(editor,
                      filePath,
                      utf8Content,
                      oldFilePath,
                      newFilePath);
    editor->beginSave();

    editor->setModified(false);
    emit editorModificationChanged(editor, false);
    if (editor == currentEditor())
    {
        emit modificationChanged(false);
    }

    setSavingTitle(editor, filePath);
    return true;
}

bool DocumentController::hasPendingSaves() const
{
    return !m_saveTransactions->isEmpty();
}

void DocumentController::dispatchAsyncSave(CustomEditor *editor,
                                           const QString &filePath,
                                           const QByteArray &utf8Content,
                                           const QString &oldFilePath,
                                           const QString &newFilePath)
{
    const auto request = m_saveTransactions->beginAsync(editor,
                                                        filePath,
                                                        oldFilePath,
                                                        newFilePath);
    m_persistence->saveFileAsync({ request.requestId,
                                   filePath,
                                   utf8Content,
                                   request.documentId });
}

void DocumentController::setSavingTitle(CustomEditor *editor, const QString &filePath)
{
    const int index = m_pane->indexOfEditor(editor);
    if (index != -1)
    {
        QString title = QFileInfo(filePath).fileName();
        if (title.isEmpty())
        {
            title = QStringLiteral("untitled.txt");
        }
        m_pane->setPageTitle(index, title + tr(" (saving)"));
    }
}

QString DocumentController::effectiveSavePath(CustomEditor *editor) const
{
    if (!editor)
        return QString();

    return m_saveTransactions->effectivePath(editor);
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

    if (!result.success)
    {
        m_editorsLoading.remove(editor);
        editor->setReadOnly(false);
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
        emit fileOpenFailed(result.filePath);
        return;
    }

    // The editor stays read-only and repaint-suppressed until the whole
    // content is in place; chunked insertion keeps the event loop alive for
    // files above the per-tick budget.
    editor->setUpdatesEnabled(false);
    QPointer<CustomEditor> editorGuard(editor);
    QPointer<DocumentPage> pageGuard(page);
    const DocumentFormat format = result.format;
    const QString filePath = result.filePath;
    const bool largeFile = result.largeFile;
    const LexerFactory::Language detectedLanguage =
        static_cast<LexerFactory::Language>(result.detectedLanguage);
    const bool activate = pending.activate;
    const bool recommendEmojiFont = isMarkdownPath(filePath)
                                    && EmojiFontSupport::shouldRecommendInstallation(
                                        result.content,
                                        EmojiFontSupport::hasSystemEmojiFont());
    if (largeFile)
        m_editorManager->promoteToLargeFileMode(editor);
    const int beforeLines = editor->lines();
    m_contentReplacer->insertInChunks(editor, result.content,
                          [this, editorGuard, pageGuard, format, filePath, largeFile, detectedLanguage, activate, recommendEmojiFont, beforeLines]() {
        if (!editorGuard)
            return;

        CustomEditor *editor = editorGuard.data();
        finalizeLoadedEditor(editor, pageGuard.data(), filePath, format, largeFile,
                             int(detectedLanguage), activate, recommendEmojiFont, beforeLines);
    });
}

void DocumentController::finalizeLoadedEditor(CustomEditor *editor,
                                              DocumentPage *page,
                                              const QString &filePath,
                                              const DocumentFormat &format,
                                              bool largeFile,
                                              int detectedLanguage,
                                              bool activate,
                                              bool recommendEmojiFont,
                                              int beforeLines)
{
    if (!editor)
        return;

    editor->notifyDocumentReplaced(editor->lines() != beforeLines);
    editor->setDocumentFormat(format);
    applyDocumentLineEnding(editor, format.lineEnding);
    m_editorManager->updateEditorSyntax(
        editor, filePath, static_cast<LexerFactory::Language>(detectedLanguage));
    editor->setUpdatesEnabled(true);
    editor->setReadOnly(false);

    if (!largeFile && AppPreferences::instance().markdownDefaultPreview() && m_markdownPreviewer)
    {
        if (auto *markdownPage = qobject_cast<MarkdownDocumentPage *>(page))
        {
            markdownPage->setInitialViewMode(true, m_markdownPreviewer, activate);
        }
    }

    setCurrentFile(editor, filePath);
    m_editorManager->updateLineNumberMarginWidth(editor);
    closeDefaultEditorIfUnused();
    if (activate)
    {
        m_pane->focusEditor(editor);
    }

    m_editorsLoading.remove(editor);
    notifyAccessibilityDocumentReplaced(editor, false);
    if (recommendEmojiFont)
    {
        emit emojiFontInstallationRecommended(filePath);
    }
    emit fileOpened(editor, filePath);
}

void DocumentController::onFileSaved(const DocumentSaveResult &result)
{
    const auto completion = m_saveTransactions->completeAsync(result.requestId);
    if (!completion.found)
    {
        return;
    }

    const auto &pending = completion.pending;
    CustomEditor *editor = pending.editor.data();
    const bool latestForEditor = completion.latestForEditor;

    if (result.superseded)
    {
        // A newer snapshot for this shared document/path remains queued. This
        // request never touched disk and must not affect editor or tab state.
        return;
    }

    if (!result.success)
    {
        const DocumentFormat currentFormat = editor ? editor->documentFormat()
                                                    : DocumentFormat{};
        if (latestForEditor && editor && pending.restoreFormatOnFailure
            && currentFormat.encoding == pending.formatAtDispatch.encoding
            && currentFormat.lineEnding == pending.formatAtDispatch.lineEnding)
        {
            editor->finishEncodingConversion(false);
        }
        if (latestForEditor && editor)
            editor->failSave();
        // An obsolete failure must not dirty a document after a newer request
        // has already superseded it.
        if (latestForEditor && editor)
            editor->markModified();

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
        if (pending.restoreFormatOnFailure)
            editor->finishEncodingConversion(true);
        // saveFile() cleared the modified flag when the snapshot was taken, so
        // any modification now means the user edited while the write was in
        // flight — the buffer no longer matches the bytes on disk.
        editor->completeSave();
        applySavedFileState(editor, result.filePath, pending.oldFilePath, pending.newFilePath);
    }
}

bool DocumentController::saveFileSynchronously(CustomEditor *editor, const QString &filePath)
{
    if (!editor)
    {
        return false;
    }
    if (!confirmExternalOverwrite(editor, filePath))
        return false;

    const QString oldFilePath = editor->filePath().isEmpty()
                                    ? QString()
                                    : QFileInfo(editor->filePath()).absoluteFilePath();
    const QString newFilePath = QFileInfo(filePath).absoluteFilePath();

    // Supersede every older async result for this shared Scintilla document,
    // including requests submitted by the other split pane to another path.
    auto transaction = m_saveTransactions->beginSynchronous(editor, newFilePath);

    // A synchronous save is the final authority for this document. Drain older
    // requests for the editor (including Save As to another path), then drain
    // requests from other panes/windows that target this destination.
    waitForPendingSaves(editor);
    m_persistence->waitForPendingSavePath(newFilePath);
    m_saveTransactions->refreshAuthority(&transaction);

    QString errorMessage;
    QByteArray utf8Content;
    if (!EditorSnapshotEncoder::encode(editor, &utf8Content, &errorMessage))
    {
        m_saveTransactions->finish(transaction);
        QMessageBox::warning(m_pane, tr("NotePeek"),
                             tr("Cannot prepare file %1 for saving:\n%2.")
                             .arg(filePath)
                             .arg(errorMessage));
        return false;
    }

    if (!DocumentIO::saveUtf8File(filePath, utf8Content, &errorMessage))
    {
        m_saveTransactions->finish(transaction);
        QMessageBox::warning(m_pane, tr("NotePeek"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(filePath)
                             .arg(errorMessage));
        return false;
    }

    editor->setModified(false);
    editor->markClean();
    applySavedFileState(editor, filePath, oldFilePath, newFilePath);
    m_saveTransactions->finish(transaction);
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
    editor->setExternalConflict(false);
    DocumentFormat savedFormat = editor->documentFormat();
    if (savedFormat.lineEnding == DocumentLineEnding::Mixed)
    {
        savedFormat.lineEnding = DocumentLineEnding::Lf;
        editor->setDocumentFormat(savedFormat);
        applyDocumentLineEnding(editor, savedFormat.lineEnding);
    }
    editor->completeSave();
    m_editorManager->updateEditorSyntax(editor, filePath);

    const QString shownName = QFileInfo(filePath).fileName();
    const int index = m_pane->indexOfEditor(editor);
    if (index != -1)
    {
        m_pane->setPageTitle(index, shownName + (editor->isModified() ? QStringLiteral("*") : QString()));
        m_pane->setPageToolTip(index, filePath);
    }

    const bool pathChanged = oldFilePath.isEmpty()
                                 ? !newFilePath.isEmpty()
                                 : !DocumentPath::equivalent(oldFilePath, newFilePath);
    if (pathChanged)
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
    // Completion results are delivered as queued signals, so waiting must
    // keep processing main-thread events. m_drainingSaves shields the
    // controller from re-entrant document mutations (external reloads, opens,
    // closes) that those events could otherwise trigger mid-drain.
    const QScopedValueRollback<bool> draining(m_drainingSaves, true);
    while (!m_saveTransactions->isEmpty())
    {
        QEventLoop loop;
        connect(m_persistence, &DocumentPersistenceService::fileSaved,
                &loop, &QEventLoop::quit);
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
}

bool DocumentController::hasPendingSave(CustomEditor *editor) const
{
    return m_saveTransactions->hasPending(editor);
}

void DocumentController::waitForPendingSaves(CustomEditor *editor)
{
    const QScopedValueRollback<bool> draining(m_drainingSaves, true);
    while (hasPendingSave(editor))
    {
        QEventLoop loop;
        connect(m_persistence, &DocumentPersistenceService::fileSaved,
                &loop, &QEventLoop::quit);
        // A very small save can finish after the condition above is checked
        // but before exec() starts, particularly on Windows. QEventLoop does
        // not remember a quit requested before it starts, so recheck from
        // inside the nested loop to avoid waiting forever for a signal that
        // has already been delivered.
        QTimer::singleShot(0, &loop, [this, editor, &loop]() {
            if (!hasPendingSave(editor))
                loop.quit();
        });
        loop.exec(QEventLoop::ExcludeUserInputEvents);
    }
}

void DocumentController::requestClose(int index)
{
    // A close arriving while another close is draining saves (event-loop
    // re-entrance) would corrupt the pane state mid-operation.
    if (m_drainingSaves)
    {
        return;
    }

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
    if (m_drainingSaves)
    {
        return false;
    }

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
        if (!editor->hasExternalConflict())
            editor->markDirty();
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
    CustomEditor *editor = m_editorManager->createEditor(QStringLiteral("untitled"));
    // No file path to infer from: apply the display policy for the empty path so
    // the untitled document honours the configured text/code default.
    m_editorManager->updateEditorSyntax(editor, QString());
    DocumentPage *page = createPage(editor, QString());
    const int tabIndex = m_pane->addPage(page, QStringLiteral("untitled"), QString());
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

    QString displayName = filePath.isEmpty() ? QStringLiteral("untitled") : QFileInfo(filePath).fileName();
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

    if (filePath.isEmpty() && !sourceEditor->isModified() && sourceEditor->length() == 0)
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

bool DocumentController::hasTrackedFile(const QString &filePath) const
{
    const QString normalizedPath = DocumentRegistry::normalizedFilePath(filePath);
    return !normalizedPath.isEmpty()
           && DocumentRegistry::trackedFilePaths(m_editorManager->getEditors()).contains(normalizedPath);
}

bool DocumentController::applyReloadedContent(const QString &filePath,
                                              const QByteArray &utf8Content,
                                              const DocumentFormat &format,
                                              bool performanceDegraded,
                                              int detectedLanguage)
{
    const QString normalizedPath = DocumentRegistry::normalizedFilePath(filePath);
    if (m_drainingSaves || normalizedPath.isEmpty() || !hasTrackedFile(normalizedPath))
    {
        return false;
    }

    if (hasModifiedEditorForFile(normalizedPath))
    {
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

        // An editor still receiving its initial chunked content must not be
        // replaced under the inserter's feet; the watcher fires again on the
        // next external change.
        if (m_contentReplacer->isReplacing(editor))
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
        const int beforeLines = editor->lines();

        const bool largeFileContent = performanceDegraded
                                      || utf8Content.size() > PerformanceConstants::LargeFileSize;
        if (largeFileContent)
            m_editorManager->promoteToLargeFileMode(editor);
        bool replaced = false;
        {
            const QSignalBlocker blocker(editor);
            replaced = m_contentReplacer->replaceWithEventLoopYield(editor, utf8Content);
            if (replaced)
            {
                editor->setDocumentFormat(format);
                applyDocumentLineEnding(editor, format.lineEnding);
                editor->setExternalConflict(false);
                editor->setModified(false);
                m_editorManager->updateEditorSyntax(
                    editor, normalizedPath, static_cast<LexerFactory::Language>(detectedLanguage));

                const int targetLine = qBound(0, line, qMax(0, editor->lines() - 1));
                const int targetIndex = qBound(0, index, editor->lineLength(targetLine));
                editor->setCursorPosition(targetLine, targetIndex);
                editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
                editor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);
                m_editorManager->updateLineNumberMarginWidth(editor);
            }
        }
        if (!replaced)
        {
            continue;
        }

        editor->notifyDocumentReplaced(editor->lines() != beforeLines);

        if (DocumentPage *page = m_pane->pageForEditor(editor))
        {
            page->refreshPreview();
        }

        notifyAccessibilityDocumentReplaced(editor, true);

        emit editorModificationChanged(editor, false);
        if (editor == currentEditor())
        {
            reloadedCurrentEditor = true;
            emit currentFileChanged(editor, normalizedPath);
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

void DocumentController::reopenCurrentWithEncoding(DocumentEncoding encoding)
{
    CustomEditor *editor = currentEditor();
    if (m_drainingSaves || !editor || editor->filePath().isEmpty()
        || m_contentReplacer->isReplacing(editor)
        || std::any_of(m_pendingEncodingReloads.cbegin(),
                       m_pendingEncodingReloads.cend(),
                       [editor](const PendingEncodingReload &pending) {
                           return pending.editor == editor;
                       }))
    {
        return;
    }

    // Reopening must observe the last successfully written snapshot. Otherwise
    // a concurrent save can finish after the read and leave disk and memory
    // representing different revisions.
    waitForPendingSaves(editor);
    if (!editor || m_drainingSaves
        || std::any_of(m_pendingEncodingReloads.cbegin(),
                       m_pendingEncodingReloads.cend(),
                       [editor](const PendingEncodingReload &pending) {
                           return pending.editor == editor;
                       }))
    {
        return;
    }

    if (editor->isModified())
    {
        const auto answer = QMessageBox::question(
            m_pane, tr("Reopen with Encoding"),
            tr("Reopening the file will discard the unsaved changes. Continue?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
        {
            return;
        }
    }

    const QString filePath = editor->filePath();
    PendingEncodingReload pending;
    pending.editor = editor;
    pending.filePath = filePath;
    pending.expectedRevision = m_editorContentRevisions.value(editor);
    pending.reloadGeneration = editor->beginEncodingReload();
    pending.wasReadOnly = editor->isReadOnly();
    editor->setReadOnly(true);
    const quint64 requestId = m_nextEncodingReloadRequestId++;
    m_pendingEncodingReloads.insert(requestId, pending);
    m_encodingReloadService->reloadFileAsync({ requestId, filePath, encoding });
}

void DocumentController::onFileReloadedWithEncoding(const DocumentEncodingReloadResult &result)
{
    const PendingEncodingReload pending = m_pendingEncodingReloads.take(result.requestId);
    CustomEditor *editor = pending.editor.data();
    if (!editor || pending.filePath.isEmpty())
        return;

    editor->setReadOnly(pending.wasReadOnly);
    if (!DocumentPath::equivalent(editor->filePath(), pending.filePath)
        || !editor->isCurrentEncodingReload(pending.reloadGeneration))
    {
        return;
    }
    // A shared view can still modify the same Scintilla document while this
    // view is read-only. Never replace a revision newer than the one the user
    // agreed to discard.
    if (m_editorContentRevisions.value(editor) != pending.expectedRevision)
        return;

    if (!result.success)
    {
        QMessageBox::warning(m_pane, tr("NotePeek"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(pending.filePath)
                             .arg(result.errorMessage));
        return;
    }

    int line = 0;
    int index = 0;
    editor->getCursorPosition(&line, &index);
    const int firstVisibleLine = editor->SendScintilla(
        QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
    const int beforeLines = editor->lines();

    const bool largeFileContent = result.performanceDegraded
                                  || result.content.size() > PerformanceConstants::LargeFileSize;
    if (largeFileContent)
        m_editorManager->promoteToLargeFileMode(editor);
    bool replaced = false;
    {
        const QSignalBlocker blocker(editor);
        replaced = m_contentReplacer->replaceWithEventLoopYield(editor, result.content);
        if (replaced)
        {
            editor->setDocumentFormat(result.format);
            applyDocumentLineEnding(editor, result.format.lineEnding);
            editor->setExternalConflict(false);
            editor->setModified(false);
            editor->markClean();
            m_editorManager->updateEditorSyntax(
                editor,
                pending.filePath,
                static_cast<LexerFactory::Language>(result.detectedLanguage));

            const int targetLine = qBound(0, line, qMax(0, editor->lines() - 1));
            editor->setCursorPosition(targetLine,
                                      qBound(0, index, editor->lineLength(targetLine)));
            editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE,
                                  firstVisibleLine);
            m_editorManager->updateLineNumberMarginWidth(editor);
        }
    }
    if (!replaced)
        return;

    editor->notifyDocumentReplaced(editor->lines() != beforeLines);

    if (DocumentPage *page = m_pane->pageForEditor(editor))
        page->refreshPreview();
    recordFileState(pending.filePath);
    notifyAccessibilityDocumentReplaced(editor, true);
    emit editorModificationChanged(editor, false);
    if (editor == currentEditor())
    {
        emit modificationChanged(false);
        emit currentFileChanged(editor, pending.filePath);
    }
}

bool DocumentController::convertEncodingAndSave(DocumentEncoding encoding)
{
    CustomEditor *editor = currentEditor();
    if (!editor)
    {
        return false;
    }

    DocumentFormat format = editor->documentFormat();
    if (format.encoding == encoding)
    {
        return true;
    }

    // The legacy code pages cover only part of Unicode; anything outside would
    // be written as a substitute character and lost on the next read.
    if (!DocumentFormatCodec::canRepresent(editor->text(), encoding))
    {
        const auto answer = QMessageBox::question(
            m_pane, tr("Convert Encoding"),
            tr("Some characters in this document cannot be represented in %1 "
               "and will be replaced. Continue?")
                .arg(DocumentFormatCodec::encodingName(encoding)),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
        {
            return false;
        }
    }

    editor->beginEncodingConversion(encoding);

    QString filePath = effectiveSavePath(editor);
    if (filePath.isEmpty())
    {
        filePath = DocumentFileDialogService::promptSaveFileName(m_pane);
        if (filePath.isEmpty())
        {
            editor->finishEncodingConversion(false);
            return false;
        }
    }

    if (!saveFile(editor, filePath))
    {
        editor->finishEncodingConversion(false);
        return false;
    }
    return true;
}

bool DocumentController::isEditorLoading(CustomEditor *editor) const
{
    return editor && m_editorsLoading.contains(editor);
}

bool DocumentController::hasModifiedEditorForFile(const QString &filePath) const
{
    return DocumentRegistry::hasModifiedEditorForFile(m_editorManager->getEditors(), filePath);
}

void DocumentController::markExternalConflict(const QString &filePath, bool deleted)
{
    for (CustomEditor *editor : m_editorManager->getEditors())
    {
        if (!editor || !DocumentPath::equivalent(editor->filePath(), filePath))
            continue;
        editor->setExternalConflict(true, deleted);
        if (deleted)
            editor->markModified();
    }
    recordFileState(filePath);
}

void DocumentController::onCurrentPageChanged(int index)
{
    if (index == -1)
    {
        emit currentEditorChanged(nullptr);
        emit modificationChanged(false);
        emit copyAvailable(false);
        return;
    }

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
        editor->updatePersistenceForModification();
        m_pane->setPageTitle(index, title + '*');
        emit modificationChanged(true);
    }
    else
    {
        editor->updatePersistenceForModification();
        m_pane->setPageTitle(index, title);
        emit modificationChanged(false);
    }

    emit editorModificationChanged(editor, editor->isModified());

    void *documentKey = editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
    if (!documentKey)
        documentKey = editor;
    if (!editor->isModified()
        && !editor->filePath().isEmpty()
        && !m_saveCoordinator->hasOutstandingDocumentSave(documentKey))
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
    m_editorContentRevisions.insert(editor, 0);
    connect(editor, &CustomEditor::textChanged, this, [this, editor]() {
        ++m_editorContentRevisions[editor];
    });
    connect(editor, &QObject::destroyed, this, [this, editor]() {
        m_editorContentRevisions.remove(editor);
        m_editorsLoading.remove(editor);
        m_contentReplacer->forgetEditor(editor);
    });
}

bool DocumentController::closeDefaultEditorIfUnused()
{
    if (!m_defaultEditor || m_defaultEditor->isModified() || m_defaultEditor->length() != 0)
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
    editor->markClean();

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
    m_externalFileWatcher->setTrackedFilePaths(this,
        DocumentRegistry::trackedFilePaths(m_editorManager->getEditors()));
}

void DocumentController::recordFileState(const QString &filePath)
{
    m_externalFileWatcher->recordFileState(filePath);
}

bool DocumentController::confirmExternalOverwrite(CustomEditor *editor, const QString &filePath)
{
    if (!editor || !editor->hasExternalConflict()
        || !DocumentPath::equivalent(editor->filePath(), filePath))
    {
        return true;
    }

    const QString detail = editor->wasDeletedExternally()
                               ? tr("The file was deleted outside NotePeek. Saving will recreate it.")
                               : tr("The file changed outside NotePeek. Saving will overwrite those changes.");
    const auto answer = QMessageBox::warning(m_pane,
                                             tr("External File Conflict"),
                                             detail,
                                             QMessageBox::Save | QMessageBox::Cancel,
                                             QMessageBox::Cancel);
    return answer == QMessageBox::Save;
}
