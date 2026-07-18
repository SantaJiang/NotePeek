# ---- QScintilla: prefer system package on Linux ----
if(UNIX AND NOT APPLE)
    set(_qscintilla_from_system OFF)

    # ---- Method 1: pkg-config (Debian/Ubuntu/Fedora provide .pc files) ----
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND AND NOT _qscintilla_from_system)
        if(NOTEPEEK_QT_MAJOR EQUAL 6)
            set(_qscintilla_pc_names "qscintilla2_qt6" "qscintilla2")
        else()
            set(_qscintilla_pc_names "qscintilla2" "qscintilla2_qt5")
        endif()

        foreach(_pc_name ${_qscintilla_pc_names})
            if(NOT _qscintilla_from_system)
                pkg_check_modules(QSCINTILLA_PC IMPORTED_TARGET "${_pc_name}" QUIET)
                if(QSCINTILLA_PC_FOUND)
                    message(STATUS "Using system qscintilla (pkg-config: ${_pc_name})")
                    add_library(qscintilla2 INTERFACE IMPORTED GLOBAL)
                    target_link_libraries(qscintilla2 INTERFACE PkgConfig::QSCINTILLA_PC)
                    set(_qscintilla_from_system ON)
                endif()
            endif()
        endforeach()
    endif()

    # ---- Method 2: find_library + find_path (Arch and others without .pc) ----
    if(NOT _qscintilla_from_system)
        if(NOTEPEEK_QT_MAJOR EQUAL 6)
            set(_qscintilla_lib_names "qscintilla2_qt6" "libqscintilla2_qt6")
        else()
            set(_qscintilla_lib_names "qscintilla2_qt5" "libqscintilla2_qt5" "qscintilla2")
        endif()

        find_library(QSCINTILLA_SYSTEM_LIB
            NAMES ${_qscintilla_lib_names}
            HINTS /usr/lib /usr/local/lib
            PATH_SUFFIXES qt6 qt5
        )

        find_path(QSCINTILLA_SYSTEM_INC
            NAMES Qsci/qsciscintilla.h
            HINTS /usr/include /usr/local/include
            PATH_SUFFIXES qt6 Qt6 qt Qt5 qt5
        )

        if(QSCINTILLA_SYSTEM_LIB AND QSCINTILLA_SYSTEM_INC)
            message(STATUS "Using system qscintilla (library: ${QSCINTILLA_SYSTEM_LIB})")
            add_library(qscintilla2 SHARED IMPORTED GLOBAL)
            set_target_properties(qscintilla2 PROPERTIES
                IMPORTED_LOCATION "${QSCINTILLA_SYSTEM_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${QSCINTILLA_SYSTEM_INC}"
            )
            target_link_libraries(qscintilla2 INTERFACE
                Qt${NOTEPEEK_QT_MAJOR}::Widgets
                Qt${NOTEPEEK_QT_MAJOR}::PrintSupport
            )
            set(_qscintilla_from_system ON)
        endif()
    endif()

    if(_qscintilla_from_system)
        set(NOTEPEEK_USE_SYSTEM_QSCINTILLA ON)
        # NotePeek ships custom lexers not present in upstream QScintilla.
        # When using the system library we still need to compile these from the bundled source.
        # Headers must also be listed so AUTOMOC processes their Q_OBJECT macros.
        set(NOTEPEEK_QSCINTILLA_EXTRA_SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/QScintilla/qscilexerqml.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/QScintilla/Qsci/qscilexerqml.h
            ${CMAKE_CURRENT_SOURCE_DIR}/QScintilla/qscilexerrust.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/QScintilla/Qsci/qscilexerrust.h
        )
    else()
        message(STATUS "System qscintilla not found, building bundled version")
        add_subdirectory(QScintilla)
        set(NOTEPEEK_USE_SYSTEM_QSCINTILLA OFF)
    endif()
else()
    add_subdirectory(QScintilla)
    set(NOTEPEEK_USE_SYSTEM_QSCINTILLA OFF)
endif()

# ---- cmark-gfm: prefer system package on Linux ----
# NotePeek links the bundled targets libcmark-gfm_static and
# libcmark-gfm-extensions_static. When a system package is found we create
# imported targets with the same names so the rest of the build is unchanged.
if(UNIX AND NOT APPLE)
    find_library(CMARK_GFM_CORE_LIB
        NAMES cmark-gfm libcmark-gfm
        HINTS /usr/lib /usr/local/lib
    )
    find_library(CMARK_GFM_EXT_LIB
        NAMES cmark-gfm-extensions libcmark-gfm-extensions
        HINTS /usr/lib /usr/local/lib
    )
    find_path(CMARK_GFM_INC
        NAMES cmark-gfm.h
        HINTS /usr/include /usr/local/include
        PATH_SUFFIXES cmark-gfm
    )

    if(CMARK_GFM_CORE_LIB AND CMARK_GFM_EXT_LIB AND CMARK_GFM_INC)
        message(STATUS "Using system cmark-gfm (library: ${CMARK_GFM_CORE_LIB})")
        add_library(libcmark-gfm_static SHARED IMPORTED GLOBAL)
        set_target_properties(libcmark-gfm_static PROPERTIES
            IMPORTED_LOCATION "${CMARK_GFM_CORE_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${CMARK_GFM_INC}"
        )
        add_library(libcmark-gfm-extensions_static SHARED IMPORTED GLOBAL)
        set_target_properties(libcmark-gfm-extensions_static PROPERTIES
            IMPORTED_LOCATION "${CMARK_GFM_EXT_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${CMARK_GFM_INC}"
        )
        set(NOTEPEEK_USE_SYSTEM_CMARK ON)
        set(NOTEPEEK_CMARK_INCLUDE_DIRS "${CMARK_GFM_INC}")
    else()
        message(STATUS "System cmark-gfm not found, building bundled version")
        set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
        set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
        set(CMARK_STATIC ON CACHE BOOL "" FORCE)
        set(CMARK_PROGRAM OFF CACHE BOOL "" FORCE)
        set(CMARK_MAN OFF CACHE BOOL "" FORCE)
        add_subdirectory(third_party/cmark-gfm EXCLUDE_FROM_ALL)
        set(NOTEPEEK_USE_SYSTEM_CMARK OFF)
        set(NOTEPEEK_CMARK_INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmark-gfm/src
            ${CMAKE_CURRENT_BINARY_DIR}/third_party/cmark-gfm/src
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmark-gfm/extensions
            ${CMAKE_CURRENT_BINARY_DIR}/third_party/cmark-gfm/extensions
        )
    endif()
else()
    set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
    set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
    set(CMARK_STATIC ON CACHE BOOL "" FORCE)
    set(CMARK_PROGRAM OFF CACHE BOOL "" FORCE)
    set(CMARK_MAN OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/cmark-gfm EXCLUDE_FROM_ALL)
    set(NOTEPEEK_USE_SYSTEM_CMARK OFF)
    set(NOTEPEEK_CMARK_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmark-gfm/src
        ${CMAKE_CURRENT_BINARY_DIR}/third_party/cmark-gfm/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmark-gfm/extensions
        ${CMAKE_CURRENT_BINARY_DIR}/third_party/cmark-gfm/extensions
    )
endif()

# ---- litehtml / gumbo-parser: prefer system packages on Linux ----
# qlitehtml ships the QLITEHTML_USE_SYSTEM_LITEHTML option; when ON it calls
# find_package(litehtml). The system litehtml target's INTERFACE_LINK_LIBRARIES
# references 'gumbo', so we must also create an imported target for the system
# gumbo-parser (Arch does not ship a CMake config for it).
if(UNIX AND NOT APPLE)
    find_library(GUMBO_SYS_LIB NAMES gumbo libgumbo)
    find_path(GUMBO_SYS_INC NAMES gumbo.h)
    if(GUMBO_SYS_LIB AND GUMBO_SYS_INC)
        # Create the gumbo target first — qlitehtml's find_package(litehtml) will
        # produce a litehtml target that transitively requires it.
        if(NOT TARGET gumbo)
            add_library(gumbo SHARED IMPORTED)
            set_target_properties(gumbo PROPERTIES
                IMPORTED_LOCATION "${GUMBO_SYS_LIB}"
                INTERFACE_INCLUDE_DIRECTORIES "${GUMBO_SYS_INC}"
            )
        endif()
        set(QLITEHTML_USE_SYSTEM_LITEHTML ON CACHE BOOL "" FORCE)
        message(STATUS "Using system litehtml + gumbo-parser (gumbo: ${GUMBO_SYS_LIB})")
    else()
        message(STATUS "System litehtml/gumbo-parser not found, building bundled versions")
    endif()
else()
    message(STATUS "Using bundled litehtml + gumbo-parser")
endif()

set(QLITEHTML_LIBRARY_TYPE STATIC)
add_subdirectory(third_party/qlitehtml)
add_subdirectory(MainPage)
