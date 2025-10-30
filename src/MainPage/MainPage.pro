QT += core gui printsupport svg network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++17

TEMPLATE = app
TARGET = notepeek

include(../Version.pri)
!versionAtLeast(QT_VERSION, 5.15):error("Qt v5.15 or greater is required")
DEFINES += APP_VERSION=\\\"$$APP_VERSION\\\"

CONFIG(release, debug|release) {
    DESTDIR = $$PWD/../Bin/Release
} else {
    DESTDIR = $$PWD/../Bin/Debug
}

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
    LIBS += -L$$DESTDIR
    LIBS += -lqscintilla2

    QMAKE_RPATHDIR += $$DESTDIR
    QMAKE_LFLAGS += -Wl,-rpath,\'\$$ORIGIN\'
}

macx {
    LIBS += -L$$DESTDIR

    CONFIG(debug, debug|release) {
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
    editor/customeditor.cpp \
    main.cpp \
    mainwindow.cpp \
    savelog.cpp

HEADERS += \
    ./editor/editormanager.h \
    ./editor/lexerfactory.h \
    ./editor/editorswitchpopup.h \
    ./editor/multieditor.h \
    ./editor/filesavedialog.h \
    ./widgets/findreplacewidget.h \
    ./widgets/qscieditorinfostatusbar.h \
    editor/customeditor.h \
    mainwindow.h \
    savelog.h

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

# 跨平台打包配置
win32 {
    package.target = package
    package.commands = \
        mkdir $$shell_path($${OUT_PWD}/package) 2>nul & \
        xcopy $$shell_path($${DESTDIR}/notepeek.exe) $$shell_path($${OUT_PWD}/package/) /Y & \
        xcopy $$shell_path($${DESTDIR}/*.dll) $$shell_path($${OUT_PWD}/package/) /Y & \
        xcopy $$shell_path($${PWD}/../../LICENSE) $$shell_path($${OUT_PWD}/package/) /Y & \
        xcopy $$shell_path($${PWD}/../../README.md) $$shell_path($${OUT_PWD}/package/) /Y

    equals(QT_MAJOR_VERSION, 6) {
        package.commands += & windeployqt --release --no-translations --no-system-d3d-compiler --no-compiler-runtime --no-opengl-sw $$shell_path($${OUT_PWD}/package/notepeek.exe)
    } else {
        package.commands += & windeployqt --release --no-translations --no-system-d3d-compiler --no-compiler-runtime --no-angle --no-opengl-sw $$shell_path($${OUT_PWD}/package/notepeek.exe)
    }

    zip.target = zip
    zip.depends = package
    zip.commands = 7z a -tzip $$quote(notepeek-v$${APP_VERSION}.zip) $$shell_path($${OUT_PWD}/package/*)

    installer.target = installer
    installer.depends = package
    installer.commands = $$shell_path($${PWD}/../../installer/bin/win/binarycreator.exe) -c $$shell_path($${PWD}/../../installer/config/config.xml) -p $$shell_path($${PWD}/../../installer/packages) $$shell_path($${OUT_PWD}/notepeek-installer.exe)

    QMAKE_EXTRA_TARGETS += package zip installer
}

macx {
    dmg.target = dmg
    dmg.commands = \
        cd $$DESTDIR && \
        macdeployqt notepeek.app -dmg && \
        mv notepeek.dmg notepeek-v$${APP_VERSION}.dmg

    QMAKE_EXTRA_TARGETS += dmg
}

linux {
    appimage.target = appimage
    appimage.commands = \
        mkdir -p $$shell_path($${OUT_PWD}/AppDir/usr/bin) && \
        mkdir -p $$shell_path($${OUT_PWD}/AppDir/usr/share/applications) && \
        mkdir -p $$shell_path($${OUT_PWD}/AppDir/usr/share/icons/hicolor/scalable/apps) && \
        cp $$shell_path($${DESTDIR}/notepeek) $$shell_path($${OUT_PWD}/AppDir/usr/bin/) && \
        cp $$shell_path($${PWD}/../../deploy/linux/notepeek.desktop) $$shell_path($${OUT_PWD}/AppDir/usr/share/applications/) && \
        cp $$shell_path($${PWD}/rescoure/notepeek.svg) $$shell_path($${OUT_PWD}/AppDir/usr/share/icons/hicolor/scalable/apps/) && \
        cd $$shell_path($${OUT_PWD}/AppDir) && \
        wget --no-verbose "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" && \
        wget --no-verbose "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" && \
        chmod +x linuxdeploy*.AppImage && \
        export QMAKE=/usr/bin/qmake && \
        export QML_SOURCES_PATHS=$$shell_path($${PWD}/../../) && \
        export OUTPUT=notepeek-v$${APP_VERSION}-x86_64.AppImage && \
        ./linuxdeploy-x86_64.AppImage --appdir . --plugin qt --output appimage

    QMAKE_EXTRA_TARGETS += appimage
}
