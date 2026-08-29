set(NOTEPEEK_QT_LIBS
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Svg
    Qt6::Concurrent
)

add_library(notepeek_core STATIC
    apppreferences.cpp
    apppreferences.h
    editor/projectpathfilter.cpp
    editor/projectpathfilter.h
    editor/documentformat.cpp
    editor/documentformat.h
    editor/documentsavecoordinator.cpp
    editor/documentsavecoordinator.h
    editor/documentsession.h
    editor/documentsession.cpp
    editor/documentio.cpp
    editor/documentio.h
    editor/documentpersistenceservice.cpp
    editor/documentpersistenceservice.h
    editor/documentencodingreloadservice.cpp
    editor/documentencodingreloadservice.h
    editor/documentregistry.cpp
    editor/documentregistry.h
    editor/supportedfilepolicy.cpp
    editor/supportedfilepolicy.h
    find/filesearcher.cpp
    find/filesearcher.h
    tool/cpp/headersourcemanager.cpp
    tool/cpp/headersourcemanager.h
    tool/format/codeformatter.cpp
    tool/format/codeformatter.h
    tool/markdown/emojifontsupport.cpp
    tool/markdown/emojifontsupport.h
    tool/markdown/markdownpreviewer.cpp
    tool/markdown/markdownpreviewer.h
    themecolors.h
    versioncontrol/gitcommandrunner.cpp
    versioncontrol/gitcommandrunner.h
    versioncontrol/gitexecutable.cpp
    versioncontrol/gitexecutable.h
    versioncontrol/gitrepositoryservice.cpp
    versioncontrol/gitrepositoryservice.h
    versioncontrol/gitstatusparser.cpp
    versioncontrol/gitstatusparser.h
    versioncontrol/gittypes.h
)

target_link_libraries(notepeek_core PUBLIC
    notepeek_foundation
    notepeek_settings
    notepeek_editor_kit
    qlitehtml
    libcmark-gfm-extensions_static
    libcmark-gfm_static
    ${NOTEPEEK_QT_LIBS}
)

target_include_directories(notepeek_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../QScintilla
    ${CMAKE_CURRENT_SOURCE_DIR}/../QScintilla/scintilla/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cmark-gfm/src
    ${CMAKE_CURRENT_BINARY_DIR}/../third_party/cmark-gfm/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cmark-gfm/extensions
    ${CMAKE_CURRENT_BINARY_DIR}/../third_party/cmark-gfm/extensions
)
notepeek_configure_project_target(notepeek_core)

add_library(notepeek_widgets STATIC
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    preferencesdialog.cpp
    preferencesdialog.h
    editor/customeditor.cpp
    editor/customeditor.h
    editor/documentcontroller.cpp
    editor/documentcontroller.h
    editor/documentpage.cpp
    editor/documentpage.h
    editor/editormanager.cpp
    editor/editormanager.h
    editor/editorcontentreplacer.cpp
    editor/editorcontentreplacer.h
    editor/documentfiledialogservice.cpp
    editor/documentfiledialogservice.h
    editor/editorsnapshotencoder.cpp
    editor/editorsnapshotencoder.h
    editor/documentsavetransactionstore.cpp
    editor/documentsavetransactionstore.h
    editor/semantichighlightingengine.cpp
    editor/semantichighlightingengine.h
    editor/editorswitchpopup.cpp
    editor/editorswitchpopup.h
    editor/externalfilewatcher.cpp
    editor/externalfilewatcher.h
    editor/filesavedialog.cpp
    editor/filesavedialog.h
    editor/editorpane.cpp
    editor/editorpane.h
    editor/editorpane.ui
    editor/spliteditorcoordinator.cpp
    editor/spliteditorcoordinator.h
    decorators/highlightedscrollbar.cpp
    decorators/highlightedscrollbar.h
    decorators/highlightedscrollbardecorator.cpp
    decorators/highlightedscrollbardecorator.h
    find/findreplacemanager.cpp
    find/findreplacemanager.h
    find/findreplacewidget.cpp
    find/findreplacewidget.h
    find/findreplacewidget.ui
    find/searchnavigationcoordinator.cpp
    find/searchnavigationcoordinator.h
    widgets/fileexplorerdock.cpp
    widgets/fileexplorerdock.h
    widgets/elidingcombobox.h
    widgets/filetreeview.cpp
    widgets/filetreeview.h
    widgets/filetypefilterproxymodel.cpp
    widgets/filetypefilterproxymodel.h
    widgets/iconfont.cpp
    widgets/iconfont.h
    widgets/qscieditorinfostatusbar.cpp
    widgets/qscieditorinfostatusbar.h
    tool/cpp/cppnavigationcoordinator.cpp
    tool/cpp/cppnavigationcoordinator.h
    tool/format/codeformattingcoordinator.cpp
    tool/format/codeformattingcoordinator.h
    tool/markdown/markdowntoolcoordinator.cpp
    tool/markdown/markdowntoolcoordinator.h
    versioncontrol/gitpresentationcoordinator.cpp
    versioncontrol/gitpresentationcoordinator.h
    widgets/searchresultsdock.cpp
    widgets/searchresultsdock.h
)

target_link_libraries(notepeek_widgets PUBLIC
    notepeek_core
)
notepeek_configure_project_target(notepeek_widgets)

add_executable(notepeek
    main.cpp
    rec.qrc
)

target_link_libraries(notepeek PRIVATE
    notepeek_widgets
)
notepeek_configure_project_target(notepeek)

target_compile_definitions(notepeek PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

# ---- Translations ----
file(GLOB_RECURSE NOTEPEEK_TRANSLATION_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ui"
)
if(Qt6_VERSION VERSION_LESS 6.7)
    qt_add_translations(notepeek
        TS_FILES ${CMAKE_SOURCE_DIR}/i18n/notepeek_zh_CN.ts
        SOURCES ${NOTEPEEK_TRANSLATION_SOURCES}
        LUPDATE_OPTIONS -no-obsolete
        RESOURCE_PREFIX "/i18n"
    )
else()
    add_custom_target(notepeek_translation_sources SOURCES ${NOTEPEEK_TRANSLATION_SOURCES})
    qt_add_translations(notepeek
        SOURCE_TARGETS notepeek_translation_sources
        TS_FILES ${CMAKE_SOURCE_DIR}/i18n/notepeek_zh_CN.ts
        LUPDATE_OPTIONS -no-obsolete
        RESOURCE_PREFIX "/i18n"
    )
endif()

# ---- Platform-specific ----

if(WIN32)
    set(NOTEPEEK_WINDOWS_ICON "${CMAKE_SOURCE_DIR}/deploy/win/notepeek.ico")
    configure_file(
        ${CMAKE_SOURCE_DIR}/deploy/win/app.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/app.rc
        @ONLY
    )
    set_target_properties(notepeek PROPERTIES WIN32_EXECUTABLE ON)
    target_sources(notepeek PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
elseif(APPLE)
    set_target_properties(notepeek PROPERTIES
        OUTPUT_NAME "NotePeek"
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
        ${CMAKE_SOURCE_DIR}/src/MainPage/resource/notepeek.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/docs/dependency-policy.md PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(notepeek PRIVATE
        ${CMAKE_SOURCE_DIR}/src/MainPage/resource/notepeek.icns
        ${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
        ${CMAKE_SOURCE_DIR}/docs/dependency-policy.md
    )
elseif(UNIX)
    set_target_properties(notepeek PROPERTIES
        BUILD_RPATH "${CMAKE_BINARY_DIR}/src/QScintilla"
        INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/notepeek"
    )
endif()

# ---- Install ----
install(TARGETS notepeek
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(NOT APPLE)
    install(FILES
        ${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
        ${CMAKE_SOURCE_DIR}/docs/dependency-policy.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/notepeek
    )
endif()

# ---- Deploy (Qt 6.3+ only) ----
# qt_generate_deploy_app_script was introduced in Qt 6.3. Linux packages use
# their declared system Qt dependencies instead of bundling Qt libraries.
if(COMMAND qt_generate_deploy_app_script AND (APPLE OR WIN32))
    qt_generate_deploy_app_script(
        TARGET notepeek
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

if(UNIX AND NOT APPLE)
    install(FILES ${CMAKE_SOURCE_DIR}/deploy/linux/notepeek.desktop
        DESTINATION share/applications
    )
    install(FILES ${CMAKE_SOURCE_DIR}/deploy/linux/notepeek.appdata.xml
        DESTINATION share/metainfo
    )
    install(FILES ${CMAKE_SOURCE_DIR}/src/MainPage/resource/notepeek.svg
        DESTINATION share/icons/hicolor/scalable/apps
    )
endif()
