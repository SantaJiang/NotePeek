#include "editor/spliteditorcoordinator.h"

#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/backgroundworkscheduler.h"
#include "editor/documentio.h"
#include "editor/documentpath.h"
#include "editor/editormanager.h"
#include "editor/filesavedialog.h"
#include "editor/editorpane.h"
#include "editor/externalfilewatcher.h"
#include "editor/performanceconstants.h"
#include <NotePeek/lexerfactory.h>

#include <QFileInfo>
#include <QFutureWatcher>
#include <QHash>
#include <QMap>
#include <QScopedValueRollback>
#include <QSet>
#include <QSplitter>
#include <QWidget>
#include <QtConcurrent>
#include <Qsci/qsciscintillabase.h>

SplitEditorCoordinator::SplitEditorCoordinator(const QList<DocumentController *> &controllers,
                                               QSplitter *splitter,
                                               bool singleFileWindow,
                                               ExternalFileWatcher *externalFileWatcher,
                                               QObject *parent)
    : QObject(parent)
    , m_controllers(controllers)
    , m_splitter(splitter)
    , m_activeController(controllers.value(0))
    , m_syncingEditorPanes(false)
    , m_splitViewEnabled(false)
    , m_singleFileWindow(singleFileWindow)
{
    for (DocumentController *controller : m_controllers)
    {
        connectController(controller);
    }
    if (externalFileWatcher)
    {
        connect(externalFileWatcher, &ExternalFileWatcher::externalFileChanged,
                this, [this](const QString &filePath) {
            reloadExternallyChangedFile(nullptr, filePath);
        });
    }
    setSplitViewEnabled(false);
}

SplitEditorCoordinator::SplitEditorCoordinator(DocumentController *leftController,
                                               DocumentController *rightController,
                                               QSplitter *splitter,
                                               bool singleFileWindow,
                                               ExternalFileWatcher *externalFileWatcher,
                                               QObject *parent)
    : SplitEditorCoordinator(QList<DocumentController *>{ leftController, rightController },
                             splitter,
                             singleFileWindow,
                             externalFileWatcher,
                             parent)
{
}

DocumentController *SplitEditorCoordinator::activeController() const
{
    return m_activeController ? m_activeController : m_controllers.value(0);
}

QList<DocumentController *> SplitEditorCoordinator::controllers() const
{
    return m_controllers;
}

void SplitEditorCoordinator::setActiveController(DocumentController *controller)
{
    if (!controller)
        return;

    m_activeController = controller;
    emit activeControllerChanged(controller);
}

void SplitEditorCoordinator::setSplitViewEnabled(bool enabled)
{
    if (m_singleFileWindow)
    {
        enabled = false;
    }

    if (!enabled)
    {
        setSingleVisibleController(visibleSingleController());
        return;
    }

    DocumentController *source = activeController();
    if (!source || !source->pane()->isVisible())
    {
        source = visibleSingleController();
    }

    m_splitViewEnabled = true;
    for (DocumentController *controller : m_controllers)
    {
        controller->pane()->setVisible(true);
        controller->pane()->setSplitViewButtonCollapseMode(true);
        controller->pane()->setSplitViewButtonVisible(true);
    }

    const QList<int> sizes = m_splitter->sizes();
    bool needsRebalance = sizes.size() < m_controllers.size();
    for (int size : sizes)
    {
        needsRebalance = needsRebalance || size == 0;
    }
    if (needsRebalance)
    {
        QList<int> equalSizes;
        equalSizes.reserve(m_controllers.size());
        for (int index = 0; index < m_controllers.size(); ++index)
        {
            equalSizes.append(1);
        }
        m_splitter->setSizes(equalSizes);
    }

    showSameFileInOtherPane(source);
}

bool SplitEditorCoordinator::closeAllEditors()
{
    for (DocumentController *controller : controllers())
    {
        controller->waitForPendingSaves();
    }

    QMap<CustomEditor *, QString> modifiedFiles;
    QHash<CustomEditor *, DocumentController *> editorOwners;
    QSet<void *> seenDocuments;

    for (DocumentController *controller : controllers())
    {
        for (CustomEditor *editor : controller->editorManager()->getEditors())
        {
            if (!editor || !editor->isModified())
            {
                continue;
            }

            void *document = editor->SendScintillaPtrResult(QsciScintillaBase::SCI_GETDOCPOINTER);
            if (document && seenDocuments.contains(document))
            {
                continue;
            }
            if (document)
            {
                seenDocuments.insert(document);
            }

            const QString filePath = editor->filePath();
            const QString displayName = filePath.isEmpty()
                                            ? QStringLiteral("untitled.txt")
                                            : QFileInfo(filePath).fileName();
            modifiedFiles.insert(editor, displayName);
            editorOwners.insert(editor, controller);
        }
    }

    if (modifiedFiles.isEmpty())
    {
        return true;
    }

    QWidget *dialogParent = qobject_cast<QWidget *>(parent());
    FileSaveDialog dialog(modifiedFiles, dialogParent);
    const int result = dialog.exec();

    if (result == QDialog::Accepted)
    {
        for (CustomEditor *editor : dialog.selectedEditors())
        {
            DocumentController *owner = editorOwners.value(editor, activeController());
            setActiveController(owner);
            if (!owner || !owner->saveEditorForClose(editor))
            {
                return false;
            }
        }

        return true;
    }

    if (result == FileSaveDialog::DontSave)
    {
        return true;
    }

    return false;
}

void SplitEditorCoordinator::reapplyThemeToEditorPanes()
{
    for (DocumentController *controller : controllers())
    {
        controller->pane()->applyChromeTheme();
        controller->editorManager()->reapplyThemeToAllEditors();
    }
}

void SplitEditorCoordinator::applyFontPreferencesToEditors()
{
    for (DocumentController *controller : controllers())
    {
        controller->editorManager()->applyFontPreferences();
    }
}

void SplitEditorCoordinator::applyCodeFeaturePreferencesToEditors()
{
    for (DocumentController *controller : controllers())
    {
        controller->editorManager()->applyCodeFeaturePreferences();
    }
}

void SplitEditorCoordinator::refreshVisibleMarkdownPreviews()
{
    for (DocumentController *controller : controllers())
    {
        controller->pane()->refreshVisibleMarkdownPreview();
    }
}

void SplitEditorCoordinator::updateActivePaneForFocus(QWidget *widget)
{
    if (!widget)
        return;

    for (DocumentController *controller : controllers())
    {
        EditorPane *pane = controller->pane();
        if (pane == widget || pane->isAncestorOf(widget))
        {
            setActiveController(controller);
            break;
        }
    }
}

DocumentController *SplitEditorCoordinator::otherController(DocumentController *controller) const
{
    const int index = m_controllers.indexOf(controller);
    if (index == -1 || m_controllers.size() < 2)
    {
        return m_controllers.value(0);
    }
    return m_controllers.at((index + 1) % m_controllers.size());
}

DocumentController *SplitEditorCoordinator::visibleSingleController() const
{
    for (DocumentController *controller : m_controllers)
    {
        if (controller->pane()->isVisible())
        {
            return controller;
        }
    }

    return m_controllers.value(0);
}

void SplitEditorCoordinator::setSingleVisibleController(DocumentController *controller)
{
    if (!controller)
    {
        controller = m_controllers.value(0);
    }

    m_splitViewEnabled = false;
    for (DocumentController *candidate : m_controllers)
    {
        const bool isTarget = candidate == controller;
        candidate->pane()->setVisible(isTarget);
        candidate->pane()->setSplitViewButtonCollapseMode(false);
        candidate->pane()->setSplitViewButtonVisible(isTarget);
    }

    setActiveController(controller);
}

void SplitEditorCoordinator::collapseSplitPane(DocumentController *controller)
{
    if (!m_splitViewEnabled)
    {
        return;
    }

    setSingleVisibleController(otherController(controller));
}

void SplitEditorCoordinator::connectController(DocumentController *controller)
{
    if (!controller)
        return;

    connect(controller, &DocumentController::currentEditorChanged, this, [this, controller](CustomEditor *) {
        if (!m_syncingEditorPanes)
        {
            setActiveController(controller);
        }
    });
    connect(controller, &DocumentController::modificationChanged, this, [this, controller](bool modified) {
        emit paneModificationChanged(controller, modified);
    });
    connect(controller, &DocumentController::currentFileChanged, this, [this, controller](CustomEditor *editor, const QString &filePath) {
        if (!m_syncingEditorPanes)
        {
            setActiveController(controller);
        }
        syncOpenedFileToOtherPanes(controller, editor, filePath);
    });
    connect(controller, &DocumentController::fileOpened, this, [this, controller](CustomEditor *editor, const QString &filePath) {
        syncOpenedFileToOtherPanes(controller, editor, filePath);
    });
    connect(controller, &DocumentController::filePathChanged, this, [this, controller](CustomEditor *editor, const QString &oldFilePath, const QString &newFilePath) {
        syncRenamedFileToOtherPanes(controller, editor, oldFilePath, newFilePath);
    });
    connect(controller, &DocumentController::externalFileChanged, this, [this, controller](const QString &filePath) {
        reloadExternallyChangedFile(controller, filePath);
    });
    connect(controller, &DocumentController::editorClosed, this, [this, controller](CustomEditor *editor) {
        syncClosedFileToOtherPanes(controller, editor ? editor->filePath() : QString());
    });
    connect(controller, &DocumentController::copyAvailable, this, [this, controller](bool available) {
        emit paneCopyAvailableChanged(controller, available);
    });
    connect(controller, &DocumentController::filesDropped, this, [this, controller](const QList<QUrl> &urls) {
        setActiveController(controller);
        emit filesDropped(controller, urls);
    });
    connect(controller->pane(), &EditorPane::splitViewToggled, this, [this, controller](bool checked) {
        setActiveController(controller);
        if (m_splitViewEnabled)
        {
            collapseSplitPane(controller);
            return;
        }

        if (checked)
        {
            setSplitViewEnabled(true);
            return;
        }

        controller->pane()->setSplitViewButtonCollapseMode(false);
    });
}

void SplitEditorCoordinator::syncOpenedFileToOtherPanes(DocumentController *source, CustomEditor *sourceEditor, const QString &filePath)
{
    if (m_syncingEditorPanes || filePath.isEmpty() || !m_splitViewEnabled)
        return;

    // Share the exact editor that carries the opened/current file. Falling back
    // to currentEditor() would mis-share when the signal refers to a file that
    // is not the active tab (e.g. a background, non-activated open).
    CustomEditor *shared = sourceEditor ? sourceEditor : source->currentEditor();
    if (!shared)
        return;

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    for (DocumentController *controller : controllers())
    {
        if (controller != source && controller->indexOfFile(filePath) == -1)
        {
            controller->openSharedDocument(shared, false);
        }
    }
}

void SplitEditorCoordinator::syncClosedFileToOtherPanes(DocumentController *source, const QString &filePath)
{
    if (m_syncingEditorPanes || filePath.isEmpty() || !m_splitViewEnabled)
        return;

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    for (DocumentController *controller : controllers())
    {
        if (controller == source)
            continue;

        const int index = controller->indexOfFile(filePath);
        if (index != -1)
        {
            controller->requestClose(index);
        }
    }
}

void SplitEditorCoordinator::syncRenamedFileToOtherPanes(DocumentController *source,
                                                         CustomEditor *sourceEditor,
                                                         const QString &oldFilePath,
                                                         const QString &newFilePath)
{
    if (m_syncingEditorPanes || newFilePath.isEmpty())
        return;

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    for (DocumentController *controller : controllers())
    {
        if (controller == source)
            continue;

        // Panes sharing the document just adopt the new path; otherwise
        // mirror the rename as open-new/close-old.
        if (controller->adoptSharedDocumentPath(sourceEditor, newFilePath))
        {
            continue;
        }

        if (controller->indexOfFile(newFilePath) == -1)
        {
            controller->openFile(newFilePath, false);
        }
        if (!oldFilePath.isEmpty()
            && !DocumentPath::equivalent(oldFilePath, newFilePath))
        {
            controller->closeFileIfUnmodified(oldFilePath);
        }
    }
}

void SplitEditorCoordinator::reloadExternallyChangedFile(DocumentController *source, const QString &filePath)
{
    if (filePath.isEmpty())
        return;

    const QString reloadKey = DocumentPath::normalized(filePath);
    if (m_syncingEditorPanes)
    {
        // A chunked apply services queued events. Remember watcher activity
        // that arrives during that nested event processing and read again once
        // the current snapshot has been fully applied.
        if (m_reloadsInFlight.contains(reloadKey))
            m_reloadRerunRequested.insert(reloadKey);
        return;
    }

    const QFileInfo info(filePath);
    const bool deleted = !info.exists() || !info.isFile();
    const bool tooLargeToReload = !deleted
                                  && info.size() > PerformanceConstants::MaximumEditableFileSize;
    bool modified = false;
    for (DocumentController *controller : controllers())
    {
        if (controller->hasModifiedEditorForFile(filePath))
        {
            modified = true;
            break;
        }
    }

    if (deleted || modified || tooLargeToReload)
    {
        for (DocumentController *controller : controllers())
            controller->markExternalConflict(filePath, deleted);
        emit externalFileConflict(filePath, deleted);
        return;
    }

    bool tracked = false;
    for (DocumentController *controller : controllers())
    {
        if (controller->hasTrackedFile(filePath))
        {
            tracked = true;
            break;
        }
    }
    if (!tracked)
        return;

    // The disk read runs off the UI thread. Only one read per path is in
    // flight; further change events for the same path queue a single re-run so
    // the newest on-disk content is never missed.
    if (m_reloadsInFlight.contains(reloadKey))
    {
        m_reloadRerunRequested.insert(reloadKey);
        return;
    }
    m_reloadsInFlight.insert(reloadKey);

    auto *watcher = new QFutureWatcher<ExternalReloadResult>(this);
    connect(watcher, &QFutureWatcher<ExternalReloadResult>::finished,
            this, [this, watcher, source, filePath, reloadKey]() {
        const ExternalReloadResult result = watcher->result();
        watcher->deleteLater();
        bool rerunRequested = m_reloadRerunRequested.remove(reloadKey);

        applyExternalReload(source, filePath, result);

        rerunRequested = m_reloadRerunRequested.remove(reloadKey) || rerunRequested;
        m_reloadsInFlight.remove(reloadKey);

        if (rerunRequested)
        {
            reloadExternallyChangedFile(nullptr, filePath);
        }
    });
    watcher->setFuture(QtConcurrent::run(BackgroundWorkScheduler::ioPool(), [filePath]() {
        ExternalReloadResult result;
        result.success = DocumentIO::loadEditorFile(filePath,
                                                    &result.content,
                                                    &result.format,
                                                    &result.errorMessage);
        if (result.success && result.content.size() <= PerformanceConstants::LargeFileSize)
        {
            result.performanceDegraded =
                PerformanceConstants::containsAbnormallyLongLine(result.content);
            if (!result.performanceDegraded)
            {
                LexerFactory::Language language = LexerFactory::languageFromFilePath(filePath);
                if (language == LexerFactory::Unknown)
                    language = LexerFactory::languageFromContent(result.content);
                result.detectedLanguage = int(language);
            }
        }
        return result;
    }));
}

void SplitEditorCoordinator::applyExternalReload(DocumentController *source,
                                                 const QString &filePath,
                                                 const ExternalReloadResult &result)
{
    if (!result.success)
    {
        for (DocumentController *controller : controllers())
            controller->markExternalConflict(filePath, false);
        emit externalFileConflict(filePath, false);
        return;
    }

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    bool reloaded = false;
    if (source)
    {
        reloaded = source->applyReloadedContent(filePath, result.content, result.format,
                                                result.performanceDegraded,
                                                result.detectedLanguage);
    }
    for (DocumentController *controller : controllers())
    {
        if (controller != source)
        {
            reloaded = controller->applyReloadedContent(filePath, result.content, result.format,
                                                        result.performanceDegraded,
                                                        result.detectedLanguage)
                       || reloaded;
        }
    }
    if (!reloaded)
    {
        for (DocumentController *controller : controllers())
            controller->markExternalConflict(filePath, false);
        emit externalFileConflict(filePath, false);
    }
}

void SplitEditorCoordinator::showSameFileInOtherPane(DocumentController *source)
{
    if (!source)
    {
        source = visibleSingleController();
    }

    DocumentController *target = otherController(source);
    CustomEditor *sourceEditor = source->currentEditor();
    if (!sourceEditor || !target)
        return;

    const auto mirrorEditorPosition = [](CustomEditor *from, CustomEditor *to) {
        if (!from || !to)
        {
            return;
        }

        int line = 0;
        int index = 0;
        from->getCursorPosition(&line, &index);
        const int firstVisibleLine = from->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
        const int xOffset = from->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET);

        const int targetLine = qBound(0, line, qMax(0, to->lines() - 1));
        const int targetIndex = qBound(0, index, to->lineLength(targetLine));
        to->setCursorPosition(targetLine, targetIndex);
        to->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
        to->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);
    };

    const QString currentFilePath = sourceEditor->filePath();
    if (currentFilePath.isEmpty())
    {
        // An untitled document is shared into the other pane so both
        // sides edit the same Scintilla document.
        target->openSharedDocument(sourceEditor, true);
        mirrorEditorPosition(sourceEditor, target->currentEditor());
        setActiveController(source);
        return;
    }

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    target->openSharedDocument(sourceEditor, true);
    mirrorEditorPosition(sourceEditor, target->currentEditor());
    setActiveController(source);
}
