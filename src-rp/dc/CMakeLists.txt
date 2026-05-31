# Library: Debug Controller (obj only)
add_library(dc INTERFACE)

target_sources(dc INTERFACE
    dc.c
)

target_include_directories(dc INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

add_subdirectory(cmd)

target_link_libraries(dc INTERFACE
    pico_stdlib
)
