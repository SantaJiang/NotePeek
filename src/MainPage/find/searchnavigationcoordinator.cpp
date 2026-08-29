#include "find/searchnavigationcoordinator.h"

#include "editor/customeditor.h"
#include "editor/documentcontroller.h"
#include "foundation/documentpath.h"
#include "editor/editormanager.h"

#include <utility>

SearchNavigationCoordinator::SearchNavigationCoordinator(
    ControllerProvider controllerProvider,
    FileReveal fileReveal,
    QObject *parent)
    : QObject(parent)
    , m_controllerProvider(std::move(controllerProvider))
    , m_fileReveal(std::move(fileReveal))
{
}

void SearchNavigationCoordinator::observeController(DocumentController *controller)
{
    if (!controller)
        return;

    connect(controller, &DocumentController::fileOpened,
            this, [this](CustomEditor *editor, const QString &filePath) {
                if (!m_pending.active
                    || !DocumentPath::equivalent(filePath, m_pending.filePath))
                {
                    return;
                }
                m_pending.active = false;
                jumpToLocation(editor, m_pending.line, m_pending.column, m_pending.length);
            });
    connect(controller, &DocumentController::fileOpenFailed,
            this, [this](const QString &filePath) {
                if (m_pending.active
                    && DocumentPath::equivalent(filePath, m_pending.filePath))
                {
                    m_pending.active = false;
                }
            });
}

void SearchNavigationCoordinator::openMatch(const QString &filePath,
                                            int line,
                                            int column,
                                            int length)
{
    DocumentController *controller = m_controllerProvider ? m_controllerProvider() : nullptr;
    if (!controller)
        return;

    CustomEditor *existingEditor = controller->editorManager()->getEditorByFilePath(filePath);
    if (existingEditor && !controller->isEditorLoading(existingEditor))
    {
        controller->openFile(filePath);
        jumpToLocation(existingEditor, line, column, length);
    }
    else
    {
        m_pending = {filePath, line, column, length, true};
        controller->openFile(filePath);
    }

    if (m_fileReveal)
        m_fileReveal(filePath);
}

void SearchNavigationCoordinator::jumpToLocation(CustomEditor *editor,
                                                 int line,
                                                 int column,
                                                 int length)
{
    if (!editor)
        return;

    const int targetLine = qBound(0, line, qMax(0, editor->lines() - 1));
    const int targetColumn = qBound(0, column, editor->lineLength(targetLine));
    editor->ensureLineVisible(targetLine);
    if (length > 0)
    {
        const int targetEnd = qBound(targetColumn,
                                     targetColumn + length,
                                     editor->lineLength(targetLine));
        editor->setSelection(targetLine, targetColumn, targetLine, targetEnd);
    }
    else
    {
        editor->setCursorPosition(targetLine, targetColumn);
    }
    editor->setFocus();
}
