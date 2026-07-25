cmake_minimum_required(VERSION 3.16)

project(NotePeek
    VERSION 1.3.3
    DESCRIPTION "A modern code/text editor"
    HOMEPAGE_URL "https://github.com/SantaSpeeds/NotePeek"
    LANGUAGES C CXX
)

set(NOTEPEEK_APP_ID "com.santa.notepeek" CACHE STRING "Reverse-DNS application identifier")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NOTEPEEK_WARNINGS_AS_ERRORS "Treat NotePeek source warnings as errors" OFF)
option(NOTEPEEK_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
set(NOTEPEEK_FORCE_QT_MAJOR "" CACHE STRING "Force Qt major version (5 or 6); empty selects Qt 6 then Qt 5")
set_property(CACHE NOTEPEEK_FORCE_QT_MAJOR PROPERTY STRINGS "" "5" "6")

function(notepeek_configure_project_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(NOTEPEEK_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(NOTEPEEK_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(NOTEPEEK_ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)
include(CTest)

# Try Qt6 first, fall back to Qt5, unless compatibility testing forces one.
if(NOTEPEEK_FORCE_QT_MAJOR STREQUAL "5")
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets PrintSupport Svg Concurrent LinguistTools)
    set(Qt6_FOUND FALSE)
    set(NOTEPEEK_QT_MAJOR 5)
elseif(NOTEPEEK_FORCE_QT_MAJOR STREQUAL "6")
    find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets PrintSupport Svg Concurrent LinguistTools)
    set(NOTEPEEK_QT_MAJOR 6)
    if(COMMAND qt_standard_project_setup)
        qt_standard_project_setup()
    endif()
else()
    if(NOT NOTEPEEK_FORCE_QT_MAJOR STREQUAL "")
        message(FATAL_ERROR "NOTEPEEK_FORCE_QT_MAJOR must be empty, 5, or 6")
    endif()
    find_package(Qt6 6.2 QUIET COMPONENTS Widgets PrintSupport Svg Concurrent LinguistTools)
    if(Qt6_FOUND)
        set(NOTEPEEK_QT_MAJOR 6)
        if(COMMAND qt_standard_project_setup)
            qt_standard_project_setup()
        endif()
    else()
        find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets PrintSupport Svg Concurrent LinguistTools)
        set(NOTEPEEK_QT_MAJOR 5)
    endif()
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt${NOTEPEEK_QT_MAJOR} QUIET COMPONENTS Test)
    if(TARGET Qt${NOTEPEEK_QT_MAJOR}::Test)
        add_subdirectory(tests)
    endif()
endif()

# ---- CPack ----
set(CPACK_PACKAGE_NAME "NotePeek")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Santa")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A modern code/text editor")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/SantaSpeeds/NotePeek")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "NotePeek")
    set(CPACK_DMG_FORMAT "UDZO")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "NotePeek")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_DISPLAY_NAME "NotePeek")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\NotePeek.lnk' '$INSTDIR\\\\${CMAKE_INSTALL_BINDIR}\\\\notepeek.exe'"
    )
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\NotePeek.lnk'"
    )
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Santa")
    set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
    set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Editors")
    if(NOTEPEEK_QT_MAJOR EQUAL 6)
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.28), libstdc++6 (>= 9), libqt6widgets6 (>= 6.2), libqt6printsupport6 (>= 6.2), libqt6svg6 (>= 6.2)")
        set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.2, qt6-qtsvg >= 6.2")
    else()
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.28), libstdc++6 (>= 9), libqt5widgets5 (>= 5.15), libqt5printsupport5 (>= 5.15), libqt5svg5 (>= 5.15)")
        set(CPACK_RPM_PACKAGE_REQUIRES "qt5-qtbase >= 5.15, qt5-qtsvg >= 5.15")
    endif()
endif()

include(CPack)
