#include "platform/platformstyle.h"
#include "themecolors.h"

#include <QApplication>
#include <QEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QSplitter>
#include <QStyle>
#include <QtTest>

class StyleChangeCounter : public QObject
{
public:
    int changes = 0;

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::StyleChange)
            ++changes;
        return QObject::eventFilter(object, event);
    }
};

class PlatformStyleTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        PlatformStyle::current().initializeApplication(*qApp);
        QCOMPARE(qApp->style()->objectName(), QStringLiteral("fusion"));
    }

    void preferencesFollowPlatformPolicy()
    {
        using ThemeColors::ColorRole;
        const auto &style = PlatformStyle::current();
        const ColorRole preferences[] = {
            ColorRole::PrefWindowBg, ColorRole::PrefPanelBg, ColorRole::PrefInputBg,
            ColorRole::PrefBorder, ColorRole::PrefText, ColorRole::PrefMutedText,
            ColorRole::PrefHoverBg, ColorRole::PrefPressedBg
        };
#ifdef Q_OS_WIN
        const ColorRole chrome[] = {
            ColorRole::TopBarPopupBg, ColorRole::ChromeBackground, ColorRole::TopBarPopupBg,
            ColorRole::TopBarBorder, ColorRole::FileTreeText, ColorRole::TopBarText,
            ColorRole::TopBarHoverBg
        };
#endif
        for (int i = 0; i < 8; ++i)
        {
            ThemeColors::RoleColors expected = ThemeColors::defaultRoleColors(preferences[i]);
#ifdef Q_OS_WIN
            expected = i == 7
                ? ThemeColors::RoleColors{qRgb(0xe8, 0xef, 0xfa), qRgb(0x3b, 0x47, 0x58)}
                : ThemeColors::defaultRoleColors(chrome[i]);
#elif defined(Q_OS_LINUX)
            if (preferences[i] == ColorRole::PrefWindowBg)
                expected = {qRgb(0xff, 0xff, 0xff), qRgb(0x26, 0x26, 0x26)};
#endif
            const auto actual = style.roleColors(preferences[i]);
            QCOMPARE(actual.light, expected.light);
            QCOMPARE(actual.dark, expected.dark);
        }

        for (auto preference : {ThemeColors::ColorSchemePreference::Light,
                                ThemeColors::ColorSchemePreference::Dark})
        {
            ThemeColors::setColorSchemePreference(preference);
#ifdef Q_OS_WIN
            QVERIFY(style.preferencesOverrides().contains(QStringLiteral("border-radius:0")));
            QVERIFY(style.preferencesOverrides().contains(ThemeColors::Preferences::pressedBg().name()));
            QVERIFY(style.fileTreeOverrides().contains(ThemeColors::EditorTopBar::border().name()));
#else
            QVERIFY(style.preferencesOverrides().isEmpty());
            QVERIFY(style.fileTreeOverrides().isEmpty());
#endif
        }
    }

    void chromeRefreshIsIdempotentAndTracksTheme()
    {
        QMainWindow window;
        auto *splitter = new QSplitter(&window);
        window.setCentralWidget(splitter);
        auto *menu = window.menuBar()->addMenu(QStringLiteral("Test"));
        menu->addAction(QStringLiteral("Item"));
        StyleChangeCounter counter;
        const QList<QObject *> styledObjects{&window, splitter, window.menuBar(), menu};
        for (QObject *object : styledObjects)
            object->installEventFilter(&counter);

        const auto &style = PlatformStyle::current();
        style.applyMainWindow(nullptr, nullptr);
        for (auto preference : {ThemeColors::ColorSchemePreference::Light,
                                ThemeColors::ColorSchemePreference::Dark})
        {
            ThemeColors::setColorSchemePreference(preference);
            style.applyMainWindow(&window, splitter);
            const QString border = ThemeColors::EditorTopBar::border().name();
            QCOMPARE(splitter->handleWidth(), 1);
            QVERIFY(splitter->styleSheet().contains(border));
            QVERIFY(window.styleSheet().contains(border));
            const int changes = counter.changes;
            style.applyMainWindow(&window, splitter);
            QCOMPARE(counter.changes, changes);
#ifndef Q_OS_WIN
#ifdef Q_OS_LINUX
            window.menuBar()->ensurePolished();
            const QColor expectedBackground = preference == ThemeColors::ColorSchemePreference::Light
                ? QColor(QStringLiteral("#ffffff")) : QColor(QStringLiteral("#2f2f2f"));
            QCOMPARE(window.menuBar()->palette().color(QPalette::Window), expectedBackground);
            QCOMPARE(window.menuBar()->palette().color(QPalette::WindowText), ThemeColors::FileTree::text());
#else
            QVERIFY(window.menuBar()->styleSheet().isEmpty());
#endif
            QVERIFY(menu->styleSheet().isEmpty());
#endif
        }
    }

    void cleanup()
    {
        ThemeColors::setColorSchemePreference(ThemeColors::ColorSchemePreference::FollowSystem);
    }
};

QTEST_MAIN(PlatformStyleTest)
#include "platformstyle_test.moc"
