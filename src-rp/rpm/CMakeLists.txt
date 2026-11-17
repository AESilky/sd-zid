# Library: Number and Base Support (obj only)
add_library(rpm INTERFACE)

target_sources(rpm INTERFACE
    rpm.c
)

target_include_directories(rpm INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/cmd
)

add_subdirectory(cmd)

target_link_libraries(rpm INTERFACE
    dbusc
    pico_stdlib
)
