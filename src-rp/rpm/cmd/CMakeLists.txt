# Library: Number and Base Support (obj only)
add_library(rpmcmds INTERFACE)

target_sources(rpmcmds INTERFACE
    cmds.c
)

target_include_directories(rpmcmds INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(rpm INTERFACE
    rpm
    pico_stdlib
)
