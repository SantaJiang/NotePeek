#ifndef PLATFORMSTYLE_H
#define PLATFORMSTYLE_H

#include <QString>

class QApplication;
class QMainWindow;
class QSplitter;

namespace ThemeColors {
enum class ColorRole;
struct RoleColors;
}

// Widgets ask for platform policy here; CMake selects exactly one native
// implementation. Shared geometry and Fusion setup live in the base class.
class PlatformStyle
{
public:
    virtual ~PlatformStyle() = default;
    static const PlatformStyle &current();

    virtual void initializeApplication(QApplication &application) const;
    virtual void applyMainWindow(QMainWindow *window, QSplitter *splitter) const;
    virtual ThemeColors::RoleColors roleColors(ThemeColors::ColorRole role) const;
    virtual QString preferencesOverrides() const;
    virtual QString fileTreeOverrides() const;
};

#endif // PLATFORMSTYLE_H
