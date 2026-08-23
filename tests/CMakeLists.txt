add_executable(notepeek_documentio_test
    documentio_test.cpp
)

target_link_libraries(notepeek_documentio_test PRIVATE
    notepeek_core
    notepeek_widgets
    Qt6::Test
)
notepeek_configure_project_target(notepeek_documentio_test)

add_test(NAME notepeek_documentio_test COMMAND notepeek_documentio_test)
set_tests_properties(notepeek_documentio_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_versioncontrol_test
    versioncontrol_test.cpp
)

target_link_libraries(notepeek_versioncontrol_test PRIVATE
    notepeek_core
    Qt6::Test
)
notepeek_configure_project_target(notepeek_versioncontrol_test)

add_test(NAME notepeek_versioncontrol_test COMMAND notepeek_versioncontrol_test)
set_tests_properties(notepeek_versioncontrol_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_editorfeatures_test
    editorfeatures_test.cpp
)

target_link_libraries(notepeek_editorfeatures_test PRIVATE
    notepeek_core
    notepeek_widgets
    Qt6::Test
)
notepeek_configure_project_target(notepeek_editorfeatures_test)

add_test(NAME notepeek_editorfeatures_test COMMAND notepeek_editorfeatures_test)
set_tests_properties(notepeek_editorfeatures_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_codeformatter_test
    codeformatter_test.cpp
)

target_link_libraries(notepeek_codeformatter_test PRIVATE
    notepeek_core
    Qt6::Test
)
notepeek_configure_project_target(notepeek_codeformatter_test)

add_test(NAME notepeek_codeformatter_test COMMAND notepeek_codeformatter_test)
set_tests_properties(notepeek_codeformatter_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_performance_test
    performance_test.cpp
)

target_link_libraries(notepeek_performance_test PRIVATE
    notepeek_core
    notepeek_widgets
    Qt6::Test
)
notepeek_configure_project_target(notepeek_performance_test)

add_test(NAME notepeek_performance_test COMMAND notepeek_performance_test)
set_tests_properties(notepeek_performance_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    LABELS "performance"
)

add_executable(notepeek_newfeatures_test
    newfeatures_test.cpp
    ${CMAKE_SOURCE_DIR}/src/MainPage/rec.qrc
)

target_link_libraries(notepeek_newfeatures_test PRIVATE
    notepeek_core
    notepeek_widgets
    Qt6::Test
)
notepeek_configure_project_target(notepeek_newfeatures_test)

add_test(NAME notepeek_newfeatures_test COMMAND notepeek_newfeatures_test)
set_tests_properties(notepeek_newfeatures_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
