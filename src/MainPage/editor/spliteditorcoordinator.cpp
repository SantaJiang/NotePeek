#include "editor/spliteditorcoordinator.h"

#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "editor/editormanager.h"
#include "editor/filesavedialog.h"
#include "editor/editorpane.h"

#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QScopedValueRollback>
#include <QSet>
#include <QSplitter>
#include <QWidget>
#include <Qsci/qsciscintillabase.h>

SplitEditorCoordinator::SplitEditorCoordinator(DocumentController *leftController,
                                               DocumentController *rightController,
                                               QSplitter *splitter,
                                               bool singleFileWindow,
                                               QObject *parent)
    : QObject(parent)
    , m_leftController(leftController)
    , m_rightController(rightController)
    , m_splitter(splitter)
    , m_activeController(leftController)
    , m_syncingEditorPanes(false)
    , m_singleFileWindow(singleFileWindow)
{
    connectController(m_leftController);
    connectController(m_rightController);
    setSplitViewEnabled(false);
}

DocumentController *SplitEditorCoordinator::activeController() const
{
    return m_activeController ? m_activeController : m_leftController;
}

QList<DocumentController *> SplitEditorCoordinator::controllers() const
{
    return { m_leftController, m_rightController };
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

    if (!enabled && activeController() == m_rightController)
    {
        setActiveController(m_leftController);
    }

    m_rightController->pane()->setVisible(enabled);
    m_leftController->pane()->setSplitViewButtonChecked(enabled);
    m_rightController->pane()->setSplitViewButtonChecked(enabled);
    m_leftController->pane()->setSplitViewButtonVisible(!enabled);
    m_rightController->pane()->setSplitViewButtonVisible(enabled);

    if (enabled)
    {
        const QList<int> sizes = m_splitter->sizes();
        if (sizes.size() < 2 || sizes.at(1) == 0)
        {
            m_splitter->setSizes({ 1, 1 });
        }
        showSameFileInRightPane();
    }
}

bool SplitEditorCoordinator::closeAllEditors()
{
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
                                            ? tr("untitled.txt")
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
        controller->editorManager()->reapplyThemeToAllEditors();
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
    connect(controller, &DocumentController::currentFileChanged, this, [this, controller](CustomEditor *, const QString &filePath) {
        if (!m_syncingEditorPanes)
        {
            setActiveController(controller);
        }
        syncOpenedFileToOtherPanes(controller, filePath);
    });
    connect(controller, &DocumentController::fileOpened, this, [this, controller](CustomEditor *, const QString &filePath) {
        syncOpenedFileToOtherPanes(controller, filePath);
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
        setSplitViewEnabled(checked);
    });
}

void SplitEditorCoordinator::syncOpenedFileToOtherPanes(DocumentController *source, const QString &filePath)
{
    if (m_syncingEditorPanes || filePath.isEmpty())
        return;

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    for (DocumentController *controller : controllers())
    {
        if (controller != source && controller->indexOfFile(filePath) == -1)
        {
            controller->openFile(filePath, false);
        }
    }
}

void SplitEditorCoordinator::syncClosedFileToOtherPanes(DocumentController *source, const QString &filePath)
{
    if (m_syncingEditorPanes || filePath.isEmpty())
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
        if (!oldFilePath.isEmpty() && oldFilePath != newFilePath)
        {
            controller->closeFileIfUnmodified(oldFilePath);
        }
    }
}

void SplitEditorCoordinator::reloadExternallyChangedFile(DocumentController *source, const QString &filePath)
{
    if (m_syncingEditorPanes || filePath.isEmpty())
        return;

    for (DocumentController *controller : controllers())
    {
        if (controller->hasModifiedEditorForFile(filePath))
        {
            return;
        }
    }

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    if (source)
    {
        source->reloadFileFromDiskIfUnmodified(filePath);
    }
    for (DocumentController *controller : controllers())
    {
        if (controller != source)
        {
            controller->reloadFileFromDiskIfUnmodified(filePath);
        }
    }
}

void SplitEditorCoordinator::showSameFileInRightPane()
{
    CustomEditor *leftEditor = m_leftController->currentEditor();
    if (!leftEditor)
        return;

    const QString currentFilePath = leftEditor->filePath();
    if (currentFilePath.isEmpty())
    {
        // An untitled document is shared into the right pane so both
        // sides edit the same Scintilla document.
        m_rightController->openSharedDocument(leftEditor, true);

        CustomEditor *rightEditor = m_rightController->currentEditor();
        if (rightEditor)
        {
            int line = 0;
            int index = 0;
            leftEditor->getCursorPosition(&line, &index);
            const int firstVisibleLine = leftEditor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
            const int xOffset = leftEditor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET);

            const int targetLine = qMin(line, rightEditor->lines() - 1);
            const int targetIndex = qMin(index, rightEditor->lineLength(targetLine));
            rightEditor->setCursorPosition(targetLine, targetIndex);
            rightEditor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
            rightEditor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);
        }

        setActiveController(m_leftController);
        return;
    }

    QScopedValueRollback<bool> syncing(m_syncingEditorPanes, true);
    for (int i = 0; i < m_leftController->pane()->count(); ++i)
    {
        const QString filePath = m_leftController->pane()->filePathAt(i);
        if (!filePath.isEmpty() && m_rightController->indexOfFile(filePath) == -1)
        {
            m_rightController->openFile(filePath, false);
        }
    }

    int line = 0;
    int index = 0;
    leftEditor->getCursorPosition(&line, &index);
    const int firstVisibleLine = leftEditor->SendScintilla(QsciScintillaBase::SCI_GETFIRSTVISIBLELINE);
    const int xOffset = leftEditor->SendScintilla(QsciScintillaBase::SCI_GETXOFFSET);

    m_rightController->openFile(currentFilePath, true);

    CustomEditor *rightEditor = m_rightController->currentEditor();
    if (rightEditor)
    {
        const int targetLine = qMin(line, rightEditor->lines() - 1);
        const int targetIndex = qMin(index, rightEditor->lineLength(targetLine));
        rightEditor->setCursorPosition(targetLine, targetIndex);
        rightEditor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, firstVisibleLine);
        rightEditor->SendScintilla(QsciScintillaBase::SCI_SETXOFFSET, xOffset);
    }
    setActiveController(m_leftController);
}
