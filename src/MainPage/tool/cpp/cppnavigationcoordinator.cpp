#include "tool/cpp/cppnavigationcoordinator.h"

#include "editor/customeditor.h"
#include "editor/documentcontroller.h"

#include <utility>

CppNavigationCoordinator::CppNavigationCoordinator(ControllerProvider controllerProvider,
                                                   FileReveal fileReveal)
    : m_controllerProvider(std::move(controllerProvider))
    , m_fileReveal(std::move(fileReveal))
{
}

bool CppNavigationCoordinator::isCppFile(const QString &filePath) const
{
    return m_headerSourceManager.isCppFile(filePath);
}

void CppNavigationCoordinator::switchHeaderSource()
{
    DocumentController *controller = m_controllerProvider ? m_controllerProvider() : nullptr;
    CustomEditor *editor = controller ? controller->currentEditor() : nullptr;
    if (!editor || editor->filePath().isEmpty())
        return;

    const QString correspondingFile =
        m_headerSourceManager.getCorrespondingFile(editor->filePath());
    if (correspondingFile.isEmpty())
        return;

    controller->openFile(correspondingFile);
    if (m_fileReveal)
        m_fileReveal(correspondingFile);
}
