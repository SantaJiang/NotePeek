file(GLOB QSCINTILLA_SOURCES_TOP CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)
# This is a NotePeek-maintained fork of QScintilla 2.14.1.  Keep the upstream
# version available for attribution and compatibility diagnostics, but give the
# shipped binary its own name and ABI so it cannot be mistaken for, or collide
# with, a system QScintilla installation.
set(NOTEPEEK_QSCINTILLA_UPSTREAM_VERSION 2.14.1)
set(NOTEPEEK_QSCINTILLA_FORK_VERSION 1.0.0)
file(GLOB QSCINTILLA_LEXERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/lexers/*.cpp
)
file(GLOB QSCINTILLA_LEXLIB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/lexlib/*.cpp
)
file(GLOB QSCINTILLA_SRC_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/src/*.cpp
)

list(FILTER QSCINTILLA_SOURCES_TOP EXCLUDE REGEX "/moc_.*[.]cpp$")
list(FILTER QSCINTILLA_SOURCES_TOP EXCLUDE REGEX "/notepeekeditor[.]cpp$")
list(FILTER QSCINTILLA_LEXERS EXCLUDE REGEX "LexLPeg")
file(GLOB QSCINTILLA_HEADERS_TOP CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Qsci/*.h
)
list(FILTER QSCINTILLA_HEADERS_TOP EXCLUDE REGEX "/notepeekeditor[.]h$")
file(GLOB QSCINTILLA_LEXLIB_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/lexlib/*.h
)
file(GLOB QSCINTILLA_SRC_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/src/*.h
)

add_library(qscintilla2 SHARED
    ${QSCINTILLA_SOURCES_TOP}
    ${QSCINTILLA_LEXERS}
    ${QSCINTILLA_LEXLIB_SOURCES}
    ${QSCINTILLA_SRC_SOURCES}
)

target_sources(qscintilla2 PRIVATE
    ${QSCINTILLA_HEADERS_TOP}
    ${QSCINTILLA_LEXLIB_HEADERS}
    ${QSCINTILLA_SRC_HEADERS}
)

target_compile_definitions(qscintilla2 PRIVATE
    QSCINTILLA_LIBRARY
    QSCINTILLA_MAKE_DLL
    SCINTILLA_QT
    SCI_LEXER
    INCLUDE_DEPRECATED_FEATURES
)

if(WIN32)
    # Every direct or transitive consumer of the shared fork must see its
    # public classes as dllimport. Keep this on the library boundary instead
    # of repeating it on selected application targets.
    target_compile_definitions(qscintilla2 INTERFACE QSCINTILLA_DLL)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(qscintilla2 PRIVATE
        -Wno-deprecated-declarations
        -Wno-deprecated-pragma
        -Wno-defaulted-function-deleted
    )
endif()

target_include_directories(qscintilla2 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Qsci>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/scintilla/include>
)
target_include_directories(qscintilla2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/lexlib
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/src
)

target_link_libraries(qscintilla2 PUBLIC
    Qt6::Widgets
    Qt6::PrintSupport
)

set_target_properties(qscintilla2 PROPERTIES
    OUTPUT_NAME NotePeekScintilla
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${NOTEPEEK_QSCINTILLA_FORK_VERSION}
    SOVERSION 1
    MACOSX_RPATH ON
    INSTALL_NAME_DIR "@rpath"
)

if(APPLE)
    set_target_properties(qscintilla2 PROPERTIES DEBUG_POSTFIX "d")
endif()

if(APPLE)
    install(TARGETS qscintilla2
        EXPORT NotePeekTargets
        LIBRARY DESTINATION NotePeek.app/Contents/Frameworks
        RUNTIME DESTINATION NotePeek.app/Contents/MacOS
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
elseif(UNIX)
    install(TARGETS qscintilla2
        EXPORT NotePeekTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/notepeek
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/notepeek
    )
else()
    install(TARGETS qscintilla2
        EXPORT NotePeekTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()
