cmake_minimum_required(VERSION 3.16)

# Keep the compiler/linker deployment target aligned with Info.plist. Merely
# setting LSMinimumSystemVersion changes the bundle metadata but does not stop
# the linker from emitting a binary that requires the build host's macOS.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING
        "Minimum supported macOS version" FORCE)
endif()

project(NotePeek
    VERSION 1.8.6
    DESCRIPTION "A modern code/text editor"
    HOMEPAGE_URL "https://github.com/SantaSpeeds/NotePeek"
    LANGUAGES C CXX
)

set(NOTEPEEK_APP_ID "com.santa.notepeek" CACHE STRING "Reverse-DNS application identifier")
set(NOTEPEEK_PACKAGE_SUFFIX "" CACHE STRING "Optional architecture-aware suffix for CPack output files")
set(NOTEPEEK_LINUX_GLIBC_MIN_VERSION "" CACHE STRING
    "glibc version declared by Linux packages; defaults to the build host")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NOTEPEEK_WARNINGS_AS_ERRORS "Treat NotePeek source warnings as errors" OFF)
option(NOTEPEEK_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(NOTEPEEK_ENABLE_COVERAGE "Instrument NotePeek targets for source coverage" OFF)
option(NOTEPEEK_ENABLE_CLANG_TIDY "Run clang-tidy on NotePeek-owned targets" OFF)

if(NOTEPEEK_ENABLE_CLANG_TIDY)
    find_program(NOTEPEEK_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
endif()

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
        if(NOTEPEEK_ENABLE_COVERAGE)
            target_compile_options(${target} PRIVATE --coverage -O0 -g)
            target_link_options(${target} PRIVATE --coverage)
        endif()
    endif()
    if(NOTEPEEK_ENABLE_CLANG_TIDY)
        set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY
            "${NOTEPEEK_CLANG_TIDY_EXECUTABLE};-checks=bugprone-*,performance-*;-warnings-as-errors=bugprone-*,performance-*")
    endif()
endfunction()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)
include(CTest)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets PrintSupport Svg Concurrent LinguistTools)
if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    find_package(Qt6 QUIET COMPONENTS Test)
    if(TARGET Qt6::Test)
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

if(NOTEPEEK_PACKAGE_SUFFIX)
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${NOTEPEEK_PACKAGE_SUFFIX}")
endif()

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
    # Rely on the explicit Qt requirements below. Ubuntu's RPM metadata
    # scanners otherwise emit invalid empty application()/mimehandler()
    # capabilities for the freedesktop metadata files.
    set(CPACK_RPM_PACKAGE_AUTOREQ "no")
    set(CPACK_RPM_PACKAGE_AUTOPROV "no")
    if(NOT NOTEPEEK_LINUX_GLIBC_MIN_VERSION)
        execute_process(
            COMMAND getconf GNU_LIBC_VERSION
            OUTPUT_VARIABLE NOTEPEEK_BUILD_GLIBC
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOTEPEEK_BUILD_GLIBC MATCHES "glibc ([0-9]+\\.[0-9]+)")
            set(NOTEPEEK_LINUX_GLIBC_MIN_VERSION "${CMAKE_MATCH_1}")
        else()
            message(FATAL_ERROR
                "Could not determine the build host glibc version. "
                "Set NOTEPEEK_LINUX_GLIBC_MIN_VERSION explicitly.")
        endif()
    endif()
    # Linux packages intentionally use the system Qt runtime. Require the Qt
    # minor used to build the package: Qt guarantees forward, not backward,
    # binary compatibility between minor releases.
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libc6 (>= ${NOTEPEEK_LINUX_GLIBC_MIN_VERSION}), libstdc++6 (>= 9), libqt6widgets6 (>= ${Qt6_VERSION}), libqt6printsupport6 (>= ${Qt6_VERSION}), libqt6svg6 (>= ${Qt6_VERSION})")
    set(CPACK_RPM_PACKAGE_REQUIRES
        "glibc >= ${NOTEPEEK_LINUX_GLIBC_MIN_VERSION}, qt6-qtbase >= ${Qt6_VERSION}, qt6-qtsvg >= ${Qt6_VERSION}")
endif()

include(CPack)
