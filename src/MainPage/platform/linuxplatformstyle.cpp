#include "platformstyle.h"

#include "themecolors.h"

#include <QMainWindow>
#include <QMenuBar>
#include <QListWidget>
#include <QStyledItemDelegate>

namespace {

class PreferencesSidebarDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        // List rows need an explicit height; stylesheet padding alone does
        // not reliably expand QListWidget's item geometry on Linux.
        size.setHeight(qMax(size.height(), qMax(36, option.fontMetrics.height() + 16)));
        return size;
    }
};

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

    ThemeColors::RoleColors roleColors(ThemeColors::ColorRole role) const override
    {
        if (role == ThemeColors::ColorRole::PrefWindowBg)
            return {qRgb(0xff, 0xff, 0xff), qRgb(0x26, 0x26, 0x26)};
        return PlatformStyle::roleColors(role);
    }

    void configurePreferencesSidebar(QListWidget *sidebar) const override
    {
        if (!sidebar)
            return;
        sidebar->setSpacing(4);
        sidebar->setItemDelegate(new PreferencesSidebarDelegate(sidebar));
    }
};

} // namespace

const PlatformStyle &PlatformStyle::current()
{
    static const LinuxPlatformStyle style;
    return style;
}
