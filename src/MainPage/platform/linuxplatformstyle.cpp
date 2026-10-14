#include "platformstyle.h"

#include "themecolors.h"

#include <QMainWindow>
#include <QMenuBar>

namespace {

class LinuxPlatformStyle final : public PlatformStyle
{
public:
    void applyMainWindow(QMainWindow *window, QSplitter *splitter) const override
    {
        PlatformStyle::applyMainWindow(window, splitter);
        if (!window)
            return;

        // Match the popup surface: white in light mode, #2f2f2f in dark mode.
        // Only override colours so Fusion retains menu sizing and spacing.
        const QString styleSheet = QStringLiteral(
            "QMenuBar { background-color:%1; color:%2; }"
            "QMenuBar::item { background-color:transparent; }"
            "QMenuBar::item:selected { background-color:%3; color:%2; }"
            "QMenuBar::item:pressed { background-color:%4; color:%2; }")
            .arg(ThemeColors::EditorTopBar::popupBg().name(),
                 ThemeColors::FileTree::text().name(),
                 ThemeColors::EditorTopBar::hoverBg().name(),
                 ThemeColors::EditorTopBar::pressedBg().name());
        QMenuBar *menuBar = window->menuBar();
        if (menuBar->styleSheet() != styleSheet)
            menuBar->setStyleSheet(styleSheet);
    }
};

} // namespace

const PlatformStyle &PlatformStyle::current()
{
    static const LinuxPlatformStyle style;
    return style;
}
