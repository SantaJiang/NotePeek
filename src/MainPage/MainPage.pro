QT += core gui printsupport
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++17

TEMPLATE = app
TARGET = notepeek

win32 {
    CONFIG(release, debug|release): {
        LIBS += -L$$PWD/../Bin/Release -lqscintilla2
    } else: CONFIG(debug, debug|release): {
        LIBS += -L$$PWD/../Bin/Debug -lqscintilla2
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
    editormanager.cpp \
    editorswitchpopup.cpp \
    filesavedialog.cpp \
    findreplacewidget.cpp \
    lexerfactory.cpp \
    main.cpp \
    mainwindow.cpp \
    multieditor.cpp \
    qscieditorinfostatusbar.cpp

HEADERS += \
    editormanager.h \
    editorswitchpopup.h \
    filesavedialog.h \
    findreplacewidget.h \
    lexerfactory.h \
    mainwindow.h \
    multieditor.h \
    qscieditorinfostatusbar.h

FORMS += \
    findreplacewidget.ui \
    mainwindow.ui \
    multieditor.ui

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
