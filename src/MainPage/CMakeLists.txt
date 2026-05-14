set(MAINPAGE_SOURCES
    main.cpp
    mainwindow.cpp
    savelog.cpp
    editor/customeditor.cpp
    editor/editormanager.cpp
    editor/editorswitchpopup.cpp
    editor/filesavedialog.cpp
    editor/lexerfactory.cpp
    editor/multieditor.cpp
    decorators/highlightedscrollbar.cpp
    decorators/highlightedscrollbardecorator.cpp
    find/findreplacemanager.cpp
    find/findreplacewidget.cpp
    widgets/qscieditorinfostatusbar.cpp
    tool/cpp/headersourcemanager.cpp
    tool/markdown/markdownpreviewer.cpp
    ../third_party/md4c/entity.c
    ../third_party/md4c/md4c.c
    ../third_party/md4c/md4c-html.c
)

set(MAINPAGE_HEADERS
    mainwindow.h
    savelog.h
    editor/customeditor.h
    editor/editormanager.h
    editor/editorswitchpopup.h
    editor/filesavedialog.h
    editor/lexerfactory.h
    editor/multieditor.h
    decorators/highlightedscrollbar.h
    decorators/highlightedscrollbardecorator.h
    find/findreplacemanager.h
    find/findreplacewidget.h
    widgets/qscieditorinfostatusbar.h
    tool/cpp/headersourcemanager.h
    tool/markdown/markdownpreviewer.h
)

set(MAINPAGE_FORMS
    mainwindow.ui
    editor/multieditor.ui
    find/findreplacewidget.ui
)

add_executable(notepeek
    ${MAINPAGE_SOURCES}
    ${MAINPAGE_HEADERS}
    ${MAINPAGE_FORMS}
    rec.qrc
)

if(Qt6_FOUND)
    target_link_libraries(notepeek PRIVATE
        qscintilla2
        Qt6::Widgets
        Qt6::PrintSupport
        Qt6::Svg
        Qt6::Network
    )
else()
    target_link_libraries(notepeek PRIVATE
        qscintilla2
        Qt5::Widgets
        Qt5::PrintSupport
        Qt5::Svg
        Qt5::Network
    )
endif()

target_include_directories(notepeek PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/md4c
    ${CMAKE_CURRENT_SOURCE_DIR}/../QScintilla
    ${CMAKE_CURRENT_SOURCE_DIR}/../QScintilla/scintilla/include
)

target_compile_definitions(notepeek PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

if(WIN32)
    target_compile_definitions(notepeek PRIVATE QSCINTILLA_DLL)
endif()

# ---- Translations ----
if(Qt6_FOUND)
    qt_add_translations(notepeek
        TS_FILES ${CMAKE_SOURCE_DIR}/i18n/notepeek_zh_CN.ts
    )
else()
    qt5_add_translation(QM_FILES
        ${CMAKE_SOURCE_DIR}/i18n/notepeek_zh_CN.ts
    )
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/translations.qrc
"<RCC>
    <qresource prefix=\"/i18n\">
        <file alias=\"notepeek_zh_CN.qm\">notepeek_zh_CN.qm</file>
    </qresource>
</RCC>")
    target_sources(notepeek PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/translations.qrc)
endif()

# ---- Platform-specific ----

if(WIN32)
    set_target_properties(notepeek PROPERTIES WIN32_EXECUTABLE ON)
    target_sources(notepeek PRIVATE ${CMAKE_SOURCE_DIR}/deploy/win/app.rc)
elseif(APPLE)
    set_target_properties(notepeek PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/deploy/mac/Info.plist
        MACOSX_BUNDLE_ICON_FILE notepeek.icns
        MACOSX_BUNDLE_BUNDLE_NAME "NotePeek"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        BUILD_RPATH "${CMAKE_BINARY_DIR}/src/QScintilla"
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/deploy/mac/Info.plist PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(notepeek PRIVATE
        ${CMAKE_SOURCE_DIR}/src/MainPage/resource/notepeek.icns
    )
elseif(UNIX)
    set_target_properties(notepeek PROPERTIES
        BUILD_RPATH "${CMAKE_BINARY_DIR}/src/QScintilla"
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
    )
endif()

# ---- Deploy (Qt6 only) ----
if(Qt6_FOUND)
    qt_generate_deploy_app_script(
        TARGET notepeek
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

# ---- Install ----
install(TARGETS notepeek
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_SOURCE_DIR}/deploy/linux/notepeek.desktop
        DESTINATION share/applications
    )
    install(FILES ${CMAKE_SOURCE_DIR}/src/MainPage/resource/notepeek.svg
        DESTINATION share/icons/hicolor/scalable/apps
    )
endif()
