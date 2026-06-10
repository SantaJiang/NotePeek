add_library(md4c STATIC
    entity.c
    md4c.c
    md4c-html.c
)

set_target_properties(md4c PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

target_include_directories(md4c PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
