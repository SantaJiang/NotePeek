#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QDebug>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    qputenv("QT_QPA_PLATFORM", "xcb");
#endif

#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication a(argc, argv);
    QCoreApplication::setApplicationName("NotePeek");
    QCoreApplication::setApplicationVersion("0.1.0");

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

    int id = QFontDatabase::addApplicationFont("://rescoure/SourceCodePro-Regular.ttf");
    if (id == -1)
    {
        qWarning() << "Failed to load font from resource";
    }

    MainWindow w;
    w.show();

    return a.exec();
}
