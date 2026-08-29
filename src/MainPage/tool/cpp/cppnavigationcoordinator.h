#ifndef CPPNAVIGATIONCOORDINATOR_H
#define CPPNAVIGATIONCOORDINATOR_H

#include "tool/cpp/headersourcemanager.h"

#include <functional>

class DocumentController;

// Coordinates the C/C++ counterpart command while HeaderSourceManager remains
// a stateless path-query service.
class CppNavigationCoordinator final
{
public:
    using ControllerProvider = std::function<DocumentController *()>;
    using FileReveal = std::function<void(const QString &)>;

    CppNavigationCoordinator(ControllerProvider controllerProvider, FileReveal fileReveal);

    bool isCppFile(const QString &filePath) const;
    void switchHeaderSource();

private:
    HeaderSourceManager m_headerSourceManager;
    ControllerProvider m_controllerProvider;
    FileReveal m_fileReveal;
};

#endif // CPPNAVIGATIONCOORDINATOR_H
