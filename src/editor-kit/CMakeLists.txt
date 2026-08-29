set(NOTEPEEK_QSCINTILLA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../QScintilla)

file(GLOB_RECURSE NOTEPEEK_EDITOR_KIT_SOURCES CONFIGURE_DEPENDS
    ${NOTEPEEK_QSCINTILLA_ROOT}/NotePeek/*.cpp
)
file(GLOB_RECURSE NOTEPEEK_EDITOR_KIT_HEADERS CONFIGURE_DEPENDS
    ${NOTEPEEK_QSCINTILLA_ROOT}/NotePeek/*.h
)

add_library(notepeek_editor_kit STATIC
    ${NOTEPEEK_EDITOR_KIT_SOURCES}
    ${NOTEPEEK_EDITOR_KIT_HEADERS}
    ${NOTEPEEK_QSCINTILLA_ROOT}/notepeekeditor.cpp
    ${NOTEPEEK_QSCINTILLA_ROOT}/Qsci/notepeekeditor.h
    ${NOTEPEEK_QSCINTILLA_ROOT}/NotePeek/notepeekthemes.qrc
)

target_link_libraries(notepeek_editor_kit PUBLIC
    qscintilla2
    Qt6::Widgets
)

target_include_directories(notepeek_editor_kit PUBLIC
    ${NOTEPEEK_QSCINTILLA_ROOT}
    ${NOTEPEEK_QSCINTILLA_ROOT}/scintilla/include
)

notepeek_configure_project_target(notepeek_editor_kit)
