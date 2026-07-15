add_executable(notepeek_documentio_test
    documentio_test.cpp
)

target_link_libraries(notepeek_documentio_test PRIVATE
    notepeek_widgets
    Qt${NOTEPEEK_QT_MAJOR}::Test
)

add_test(NAME notepeek_documentio_test COMMAND notepeek_documentio_test)
set_tests_properties(notepeek_documentio_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_editorfeatures_test
    editorfeatures_test.cpp
)

target_link_libraries(notepeek_editorfeatures_test PRIVATE
    notepeek_widgets
    Qt${NOTEPEEK_QT_MAJOR}::Test
)

add_test(NAME notepeek_editorfeatures_test COMMAND notepeek_editorfeatures_test)
set_tests_properties(notepeek_editorfeatures_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
