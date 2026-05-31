# Library: cmd (command shell) (obj only)
add_library(cmd INTERFACE)

target_sources(cmd INTERFACE
    cmd.c
)

target_include_directories(cmd INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

target_link_libraries(cmd INTERFACE
    shell
    pico_stdlib
)
