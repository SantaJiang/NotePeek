#ifndef SEARCHNAVIGATIONCOORDINATOR_H
#define SEARCHNAVIGATIONCOORDINATOR_H

#include <QObject>
#include <QString>

#include <functional>

class CustomEditor;
class DocumentController;

// Coordinates project-search result navigation across asynchronous document
// loads. SearchResultsDock remains the view and DocumentController remains the
// document owner.
class SearchNavigationCoordinator final : public QObject
{
    Q_OBJECT

public:
    using ControllerProvider = std::function<DocumentController *()>;
    using FileReveal = std::function<void(const QString &)>;

    explicit SearchNavigationCoordinator(ControllerProvider controllerProvider,
                                         FileReveal fileReveal,
                                         QObject *parent = nullptr);

    void observeController(DocumentController *controller);
    void openMatch(const QString &filePath, int line, int column, int length);

private:
    struct PendingLocation
    {
        QString filePath;
        int line = 0;
        int column = 0;
        int length = 0;
        bool active = false;
    };

    static void jumpToLocation(CustomEditor *editor, int line, int column, int length);

    ControllerProvider m_controllerProvider;
    FileReveal m_fileReveal;
    PendingLocation m_pending;
};

#endif // SEARCHNAVIGATIONCOORDINATOR_H
