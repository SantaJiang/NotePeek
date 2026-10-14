#include "platformstyle.h"

#include "themecolors.h"

#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QStyleFactory>

namespace {

void applyEditorSplitterStyle(QSplitter *splitter)
{
    if (!splitter)
    {
        return;
    }

    splitter->setHandleWidth(1);
    const QString styleSheet = QStringLiteral(
                                "QSplitter::handle:horizontal {"
                                "  width: 1px;"
                                "  background-color: %1;"
                                "  border: 0;"
                                "  margin: 0;"
                                "  padding: 0;"
                                "}").arg(ThemeColors::EditorTopBar::border().name());
    if (splitter->styleSheet() != styleSheet)
    {
        splitter->setStyleSheet(styleSheet);
    }
}

void applyMainWindowSeparatorStyle(QMainWindow *window)
{
    if (!window)
    {
        return;
    }

    const QString separatorColor = ThemeColors::EditorTopBar::border().name();
    const QString styleSheet = QStringLiteral(
                              "QMainWindow::separator {"
                              "  background-color: %1;"
                              "  width: 1px;"
                              "  height: 1px;"
                              "  border: 0;"
                              "  margin: 0;"
                              "  padding: 0;"
                              "}"
                              "QMainWindow::separator:hover {"
                              "  background-color: %1;"
                              "}")
                              .arg(separatorColor);
    if (window->styleSheet() != styleSheet)
    {
        window->setStyleSheet(styleSheet);
    }
}

} // namespace

void PlatformStyle::initializeApplication(QApplication &application) const
{
    if (QStyle *style = QStyleFactory::create(QStringLiteral("Fusion")))
        application.setStyle(style);
}

void PlatformStyle::applyMainWindow(QMainWindow *window, QSplitter *splitter) const
{
    applyEditorSplitterStyle(splitter);
    applyMainWindowSeparatorStyle(window);
}

ThemeColors::RoleColors PlatformStyle::roleColors(ThemeColors::ColorRole role) const
{
    return ThemeColors::defaultRoleColors(role);
}

QString PlatformStyle::preferencesOverrides() const
{
    return {};
}

QString PlatformStyle::fileTreeOverrides() const
{
    return {};
}
