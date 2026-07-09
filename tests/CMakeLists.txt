add_executable(notepeek_documentio_test
    documentio_test.cpp
)

target_link_libraries(notepeek_documentio_test PRIVATE
    notepeek_core
    Qt${NOTEPEEK_QT_MAJOR}::Test
)

add_test(NAME notepeek_documentio_test COMMAND notepeek_documentio_test)
