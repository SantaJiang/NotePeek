#include "mainwindow.h"
#include "apppreferences.h"
#include <QApplication>
#include <QTranslator>
#include <QDebug>
#include <QStyleFactory>
#include <QFontDatabase>
#include <QEvent>
#include <QFileOpenEvent>
#include <QCommandLineOption>
#include <QCommandLineParser>

class MacFileOpenEventFilter : public QObject
{
public:
    explicit MacFileOpenEventFilter(MainWindow *window, QObject *parent = nullptr)
        : QObject(parent)
        , m_window(window)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched);
        if (event->type() == QEvent::FileOpen && m_window != nullptr)
        {
            auto *fileOpenEvent = static_cast<QFileOpenEvent *>(event);
            const QString filePath = fileOpenEvent->file();
            if (!filePath.isEmpty())
            {
                m_window->openFileByArg(filePath);
            }
            return true;
        }

        return QObject::eventFilter(watched, event);
    }

private:
    MainWindow *m_window = nullptr;
};

int main(int argc, char *argv[])
{
    //high dpi
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("NotePeek");
    QCoreApplication::setApplicationName("NotePeek");
    QCoreApplication::setApplicationVersion(APP_VERSION);

    QCommandLineParser commandLine;
    commandLine.setApplicationDescription(QStringLiteral("A modern code and text editor"));
    commandLine.addHelpOption();
    commandLine.addVersionOption();
    const QCommandLineOption singleFileWindowOption(
        QStringLiteral("single-file-window"),
        QStringLiteral("Open the requested files in a window without split controls."));
    commandLine.addOption(singleFileWindowOption);
    commandLine.addPositionalArgument(QStringLiteral("files"),
                                      QStringLiteral("Files to open."),
                                      QStringLiteral("[files...]"));
    commandLine.process(a);

    //style
#ifdef Q_OS_WIN
    if (QStyle *style = QStyleFactory::create("Fusion"))
        a.setStyle(style);
#elif defined(Q_OS_MAC)
    if (QStyle *style = QStyleFactory::create("Fusion"))
        a.setStyle(style);
#elif defined(Q_OS_LINUX)
    if (QStyle *style = QStyleFactory::create("Fusion"))
        a.setStyle(style);
#else
    qDebug() << "Running on unknown platform";
#endif

    //translate
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages)
    {
        const QString baseName = QStringLiteral("notepeek_") + QLocale(locale).name();
        if (translator.load(QStringLiteral(":/i18n/") + baseName))
        {
            a.installTranslator(&translator);
            break;
        }
    }

    //font — install the bundled families so they are available as editor fonts
    //regardless of what the host system ships.
    if (QFontDatabase::addApplicationFont(QStringLiteral("://resource/SourceCodePro-Regular.ttf")) == -1)
    {
        qWarning() << "Failed to load Source Code Pro from resource";
    }
    if (QFontDatabase::addApplicationFont(QStringLiteral("://resource/FiraCode-Regular.ttf")) == -1)
    {
        qWarning() << "Failed to load Fira Code from resource";
    }
    // Bundle a color-emoji family so glyphs like 📄 render even on systems that
    // ship no emoji font (common on Linux).  It sits last in the Markdown font
    // fallback stacks, behind the platform-native emoji fonts on macOS/Windows.
    if (QFontDatabase::addApplicationFont(QStringLiteral("://resource/NotoColorEmoji.ttf")) == -1)
    {
        qWarning() << "Failed to load Noto Color Emoji from resource";
    }

    // Load persisted user preferences after the fonts are registered so font
    // validation sees the bundled families.
    AppPreferences::instance().load();

    MainWindow w(nullptr,
                 commandLine.positionalArguments(),
                 commandLine.isSet(singleFileWindowOption));
    MacFileOpenEventFilter fileOpenEventFilter(&w, &a);
    a.installEventFilter(&fileOpenEventFilter);
    w.show();

    return a.exec();
}
