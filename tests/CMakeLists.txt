add_executable(notepeek_documentio_test
    documentio_test.cpp
)

target_link_libraries(notepeek_documentio_test PRIVATE
    notepeek_core
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
    notepeek_widgets
    Qt6::Test
)
notepeek_configure_project_target(notepeek_editorfeatures_test)

add_test(NAME notepeek_editorfeatures_test COMMAND notepeek_editorfeatures_test)
set_tests_properties(notepeek_editorfeatures_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_editorbulkload_test
    editorbulkload_test.cpp
)

target_link_libraries(notepeek_editorbulkload_test PRIVATE
    notepeek_widgets
    Qt6::Test
)
notepeek_configure_project_target(notepeek_editorbulkload_test)

add_test(NAME notepeek_editorbulkload_test COMMAND notepeek_editorbulkload_test)
set_tests_properties(notepeek_editorbulkload_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_documentoptions_test
    documentoptions_test.cpp
)

target_link_libraries(notepeek_documentoptions_test PRIVATE
    notepeek_widgets
    Qt6::Test
)
notepeek_configure_project_target(notepeek_documentoptions_test)

add_test(NAME notepeek_documentoptions_test COMMAND notepeek_documentoptions_test)
set_tests_properties(notepeek_documentoptions_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_documentstreamdecoder_test
    documentstreamdecoder_test.cpp
)

target_link_libraries(notepeek_documentstreamdecoder_test PRIVATE
    notepeek_core
    Qt6::Test
)
notepeek_configure_project_target(notepeek_documentstreamdecoder_test)

add_test(NAME notepeek_documentstreamdecoder_test COMMAND notepeek_documentstreamdecoder_test)
set_tests_properties(notepeek_documentstreamdecoder_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_streamingdocumentloader_test
    streamingdocumentloader_test.cpp
)

target_link_libraries(notepeek_streamingdocumentloader_test PRIVATE
    notepeek_widgets
    Qt6::Test
)
notepeek_configure_project_target(notepeek_streamingdocumentloader_test)

add_test(NAME notepeek_streamingdocumentloader_test COMMAND notepeek_streamingdocumentloader_test)
set_tests_properties(notepeek_streamingdocumentloader_test PROPERTIES
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

add_executable(notepeek_settings_test
    settings_test.cpp
)

target_link_libraries(notepeek_settings_test PRIVATE
    notepeek_core
    Qt6::Test
)
notepeek_configure_project_target(notepeek_settings_test)

add_test(NAME notepeek_settings_test COMMAND notepeek_settings_test)
set_tests_properties(notepeek_settings_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_documentsession_test
    documentsession_test.cpp
)

target_link_libraries(notepeek_documentsession_test PRIVATE
    notepeek_core
    Qt6::Test
)
notepeek_configure_project_target(notepeek_documentsession_test)

add_test(NAME notepeek_documentsession_test COMMAND notepeek_documentsession_test)
set_tests_properties(notepeek_documentsession_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_executable(notepeek_performance_test
    performance_test.cpp
)

target_link_libraries(notepeek_performance_test PRIVATE
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
    notepeek_widgets
    Qt6::Test
)
notepeek_configure_project_target(notepeek_newfeatures_test)

add_test(NAME notepeek_newfeatures_test COMMAND notepeek_newfeatures_test)
set_tests_properties(notepeek_newfeatures_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME notepeek_architecture_test
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_SOURCE_DIR}/scripts/check_architecture.py
                --root ${CMAKE_SOURCE_DIR}
    )
endif()

add_executable(notepeek_platformstyle_test
    platformstyle_test.cpp
)
target_link_libraries(notepeek_platformstyle_test PRIVATE
    notepeek_core
    Qt6::Test
)
notepeek_configure_project_target(notepeek_platformstyle_test)
add_test(NAME notepeek_platformstyle_test COMMAND notepeek_platformstyle_test)
set_tests_properties(notepeek_platformstyle_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
