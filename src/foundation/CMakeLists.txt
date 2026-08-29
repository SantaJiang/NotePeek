add_library(notepeek_foundation STATIC
    backgroundworkscheduler.cpp
    backgroundworkscheduler.h
    documentpath.cpp
    documentpath.h
    performanceconstants.h
)

target_link_libraries(notepeek_foundation PUBLIC
    Qt6::Core
)

target_include_directories(notepeek_foundation PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

notepeek_configure_project_target(notepeek_foundation)
