#include "platformstyle.h"

#include "themecolors.h"

#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

// Keep native macros confined to this translation unit.
#include <qt_windows.h>

namespace {

void applyWindowsMenuColors(QMainWindow *window)
{
    if (!window)
        return;

    // Colour-only styling: preserve Fusion's sizing, fonts and menu geometry.
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    const bool highContrast = SystemParametersInfoW(
        SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)
        && (contrast.dwFlags & HCF_HIGHCONTRASTON);
    QString styleSheet;
    if (!highContrast)
    {
        styleSheet = QStringLiteral(
            "QMenuBar { background-color:%1; color:%2; }"
            "QMenuBar::item { background-color:transparent; }"
            "QMenuBar::item:selected, QMenuBar::item:pressed { background-color:%3; color:%2; }"
            "QMenu { background-color:%4; color:%2; }"
            "QMenu::item:selected { background-color:%3; color:%2; }"
            "QMenu::item:disabled { color:%5; }")
            .arg(ThemeColors::EditorTopBar::background().name(),
                 ThemeColors::FileTree::text().name(),
                 ThemeColors::Preferences::pressedBg().name(),
                 ThemeColors::EditorTopBar::popupBg().name(),
                 ThemeColors::EditorTopBar::mutedText().name());
    }
    if (window->menuBar()->styleSheet() != styleSheet)
        window->menuBar()->setStyleSheet(styleSheet);
    for (QMenu *menu : window->findChildren<QMenu *>())
    {
        if (menu->styleSheet() != styleSheet)
            menu->setStyleSheet(styleSheet);
    }
}

class WindowsPlatformStyle final : public PlatformStyle
{
public:
    void applyMainWindow(QMainWindow *window, QSplitter *splitter) const override
    {
        PlatformStyle::applyMainWindow(window, splitter);
        applyWindowsMenuColors(window);
    }

    ThemeColors::RoleColors roleColors(ThemeColors::ColorRole role) const override
    {
        using ThemeColors::ColorRole;
        switch (role)
        {
        case ColorRole::PrefWindowBg:
        case ColorRole::PrefInputBg: role = ColorRole::TopBarPopupBg; break;
        case ColorRole::PrefPanelBg: role = ColorRole::ChromeBackground; break;
        case ColorRole::PrefBorder: role = ColorRole::TopBarBorder; break;
        case ColorRole::PrefText: role = ColorRole::FileTreeText; break;
        case ColorRole::PrefMutedText: role = ColorRole::TopBarText; break;
        case ColorRole::PrefHoverBg: role = ColorRole::TopBarHoverBg; break;
        case ColorRole::PrefPressedBg:
            return {qRgb(0xe8,0xef,0xfa), qRgb(0x3b,0x47,0x58)};
        default: break;
        }
        return PlatformStyle::roleColors(role);
    }

    QString preferencesOverrides() const override
    {
        using namespace ThemeColors::Preferences;
        return QStringLiteral(
            "QLineEdit#settingsSearch, QPushButton, QListWidget#sectionList::item { border-radius:0; }"
            "QListWidget#sectionList::item:selected { background-color:%1; color:%2; }"
            "QAbstractItemView { selection-background-color:%1; selection-color:%2; }"
            "QAbstractItemView::item:selected { background-color:%1; color:%2; }")
            .arg(pressedBg().name(QColor::HexRgb), text().name(QColor::HexRgb));
    }

    QString fileTreeOverrides() const override
    {
        return QStringLiteral("QTreeView { border-top: 1px solid %1; }")
            .arg(ThemeColors::EditorTopBar::border().name(QColor::HexRgb));
    }
};

} // namespace

const PlatformStyle &PlatformStyle::current()
{
    static const WindowsPlatformStyle style;
    return style;
}
