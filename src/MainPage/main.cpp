#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QDebug>
#include <QStyleFactory>
#include <QStandardPaths>
#include "savelog.h"

QString getLogDirectory()
{
    QString logDir;

#if defined(Q_OS_WIN)
    // Windows: AppData/Roaming/AppName/log
    logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/log";

#elif defined(Q_OS_MAC)
    // macOS: ~/Library/Logs/AppName
    logDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Library/Log";

#elif defined(Q_OS_LINUX)
    // Linux: ~/.local/share/AppName/log
    logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/log";

#else
    // 其他平台使用AppDataLocation
    logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/log";
#endif

    return logDir;
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

    //high dpi
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("NotePeek");
    QCoreApplication::setApplicationName("NotePeek");
    QCoreApplication::setApplicationVersion(APP_VERSION);

#ifdef QT_NO_DEBUG
    SaveLog::Instance()->setUseContext(false);
    SaveLog::Instance()->setPath(getLogDirectory());
    //SaveLog::Instance()->start();
#endif

    //style
    QStringList styles = QStyleFactory::keys();
    qDebug() << "Available styles:" << styles;
#ifdef Q_OS_WIN
    a.setStyle(QStyleFactory::create("windowsvista"));
#elif defined(Q_OS_MAC)
    a.setStyle(QStyleFactory::create("Fusion"));
#elif defined(Q_OS_LINUX)
    a.setStyle(QStyleFactory::create("Fusion"));
#else
    qDebug() << "Running on unknown platform";
#endif

    //translate
    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for(const QString &locale : uiLanguages)
    {
        const QString baseName = "notepeek_" + QLocale(locale).name();
        if(translator.load(":/i18n/" + baseName))
        {
            a.installTranslator(&translator);
            break;
        }
    }

    //font
    int id = QFontDatabase::addApplicationFont("://rescoure/SourceCodePro-Regular.ttf");
    if (id == -1)
    {
        qWarning() << "Failed to load font from resource";
    }

    MainWindow w;
    w.show();

    return a.exec();
}
