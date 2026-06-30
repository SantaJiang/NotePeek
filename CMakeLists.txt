cmake_minimum_required(VERSION 3.16)

project(NotePeek
    VERSION 1.1.6
    DESCRIPTION "A modern code/text editor"
    HOMEPAGE_URL "https://github.com/SantaSpeeds/NotePeek"
    LANGUAGES C CXX
)

set(NOTEPEEK_APP_ID "com.santa.notepeek" CACHE STRING "Reverse-DNS application identifier")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

# Try Qt6 first, fall back to Qt5
find_package(Qt6 QUIET COMPONENTS Widgets PrintSupport Svg LinguistTools)
if(Qt6_FOUND)
    set(NOTEPEEK_QT_MAJOR 6)
    qt_standard_project_setup()
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets PrintSupport Svg LinguistTools)
    set(NOTEPEEK_QT_MAJOR 5)
endif()

add_subdirectory(src)

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
