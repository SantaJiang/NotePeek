add_library(notepeek_settings STATIC
    settingsrepository.cpp
    settingsrepository.h
)

target_link_libraries(notepeek_settings PUBLIC
    Qt6::Core
)

target_include_directories(notepeek_settings PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

notepeek_configure_project_target(notepeek_settings)
