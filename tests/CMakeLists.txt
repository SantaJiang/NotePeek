add_executable(notepeek_documentio_test
    documentio_test.cpp
)

target_link_libraries(notepeek_documentio_test PRIVATE
    notepeek_widgets
    Qt${NOTEPEEK_QT_MAJOR}::Test
)
notepeek_configure_project_target(notepeek_documentio_test)

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
notepeek_configure_project_target(notepeek_editorfeatures_test)

add_test(NAME notepeek_editorfeatures_test COMMAND notepeek_editorfeatures_test)
set_tests_properties(notepeek_editorfeatures_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_performance_test
    performance_test.cpp
)

target_link_libraries(notepeek_performance_test PRIVATE
    notepeek_widgets
    Qt${NOTEPEEK_QT_MAJOR}::Test
)
notepeek_configure_project_target(notepeek_performance_test)

add_test(NAME notepeek_performance_test COMMAND notepeek_performance_test)
set_tests_properties(notepeek_performance_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "performance"
)

add_executable(notepeek_newfeatures_test
    newfeatures_test.cpp
)

target_link_libraries(notepeek_newfeatures_test PRIVATE
    notepeek_widgets
    Qt${NOTEPEEK_QT_MAJOR}::Test
)
notepeek_configure_project_target(notepeek_newfeatures_test)

add_test(NAME notepeek_newfeatures_test COMMAND notepeek_newfeatures_test)
set_tests_properties(notepeek_newfeatures_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
