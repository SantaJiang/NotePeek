# Prefer a system-provided md4c when available (e.g. Arch's md4c package),
# otherwise build the bundled sources. The exported target name is always
# `md4c` so consumers don't need to care which one is used.
find_package(md4c QUIET)

if(md4c_FOUND AND TARGET md4c::md4c)
    message(STATUS "Using system md4c")
    add_library(md4c INTERFACE IMPORTED GLOBAL)
    target_link_libraries(md4c INTERFACE md4c::md4c md4c::md4c-html)
else()
    message(STATUS "System md4c not found, building bundled version")
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
endif()
