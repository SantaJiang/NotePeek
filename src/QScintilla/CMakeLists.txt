file(GLOB QSCINTILLA_SOURCES_TOP CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)
file(GLOB QSCINTILLA_LEXERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/lexers/*.cpp
)
file(GLOB QSCINTILLA_LEXLIB_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/lexlib/*.cpp
)
file(GLOB QSCINTILLA_SRC_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/scintilla/src/*.cpp
)

list(FILTER QSCINTILLA_SOURCES_TOP EXCLUDE REGEX "/moc_.*\\\\.cpp$")
list(FILTER QSCINTILLA_LEXERS EXCLUDE REGEX "LexLPeg")
if(NOT APPLE)
    list(FILTER QSCINTILLA_SOURCES_TOP EXCLUDE REGEX "MacPasteboardMime")
endif()

file(GLOB QSCINTILLA_HEADERS_TOP CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Qsci/*.h
)
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

if(Qt6_FOUND)
    target_link_libraries(qscintilla2 PUBLIC
        Qt6::Widgets
        Qt6::PrintSupport
    )
else()
    target_link_libraries(qscintilla2 PUBLIC
        Qt5::Widgets
        Qt5::PrintSupport
    )
    if(APPLE)
        find_package(Qt5MacExtras QUIET)
        if(Qt5MacExtras_FOUND)
            target_link_libraries(qscintilla2 PUBLIC Qt5::MacExtras)
        endif()
    endif()
endif()

set_target_properties(qscintilla2 PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 15
    MACOSX_RPATH ON
    INSTALL_NAME_DIR "@rpath"
)

if(APPLE)
    set_target_properties(qscintilla2 PROPERTIES DEBUG_POSTFIX "d")
endif()

if(APPLE)
    install(TARGETS qscintilla2
        EXPORT NotePeekTargets
        LIBRARY DESTINATION notepeek.app/Contents/Frameworks
        RUNTIME DESTINATION notepeek.app/Contents/MacOS
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
else()
    install(TARGETS qscintilla2
        EXPORT NotePeekTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()
