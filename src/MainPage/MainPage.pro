QT += core gui printsupport svg
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++17

TEMPLATE = app
TARGET = notepeek

win32 {
    DEFINES += QSCINTILLA_DLL
    DEFINES -= QSCINTILLA_MAKE_DLL

    CONFIG(debug, debug|release) {
        LIBS += -L$$PWD/../Bin/Debug -lqscintilla2
    } else {
        LIBS += -L$$PWD/../Bin/Release -lqscintilla2
    }

    RC_FILE += app.rc
}

unix:!macx {
    CONFIG(release, debug|release): {
        DESTDIR = $$PWD/../Bin/Release
    } else: CONFIG(debug, debug|release): {
        DESTDIR = $$PWD/../Bin/Debug
    }

    LIBS += -L$$DESTDIR
    LIBS += -lqscintilla2

    QMAKE_RPATHDIR += $$DESTDIR
    QMAKE_LFLAGS += -Wl,-rpath,\'\$$ORIGIN\'
}

macx {
    CONFIG(release, debug|release): {
        DESTDIR = $$PWD/../Bin/Release
    } else: CONFIG(debug, debug|release): {
        DESTDIR = $$PWD/../Bin/Debug
    }

    LIBS += -L$$DESTDIR

    CONFIG(debug, debug|release): {
        LIBS += -lqscintilla2d
    } else {
        LIBS += -lqscintilla2
    }

    QMAKE_RPATHDIR += $$DESTDIR
    QMAKE_LFLAGS += -Wl,-rpath,\'\$$ORIGIN\'
    ICON = $$PWD/rescoure/notepeek.icns
}

INCLUDEPATH += $$PWD/../QScintilla \
               $$PWD/../QScintilla/Qsci \
               $$PWD/../QScintilla/scintilla/include

SOURCES += \
    ./editor/editormanager.cpp \
    ./editor/lexerfactory.cpp \
    ./editor/editorswitchpopup.cpp \
    ./editor/multieditor.cpp \
    ./editor/filesavedialog.cpp \
    ./widgets/findreplacewidget.cpp \
    ./widgets/qscieditorinfostatusbar.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    ./editor/editormanager.h \
    ./editor/lexerfactory.h \
    ./editor/editorswitchpopup.h \
    ./editor/multieditor.h \
    ./editor/filesavedialog.h \
    ./widgets/findreplacewidget.h \
    ./widgets/qscieditorinfostatusbar.h \
    mainwindow.h

FORMS += \
    ./widgets/findreplacewidget.ui \
    mainwindow.ui \
    ./editor/multieditor.ui

TRANSLATIONS = \
    $$PWD/../../i18n/notepeek_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

RESOURCES += \
    rec.qrc

isEmpty(target.path) {
    qnx: target.path = /tmp/$${TARGET}/bin
    else: unix:!android: target.path = /opt/$${TARGET}/bin
    !isEmpty(target.path): INSTALLS += target
}
