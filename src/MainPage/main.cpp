#include "mainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QDebug>
#include <QStyleFactory>
#include <QFontDatabase>
#include <QEvent>
#include <QFileOpenEvent>

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

    //font
    int id = QFontDatabase::addApplicationFont(QStringLiteral("://resource/SourceCodePro-Regular.ttf"));
    if (id == -1)
    {
        qWarning() << "Failed to load font from resource";
    }

    MainWindow w;
    MacFileOpenEventFilter fileOpenEventFilter(&w, &a);
    a.installEventFilter(&fileOpenEventFilter);
    w.show();

    return a.exec();
}
