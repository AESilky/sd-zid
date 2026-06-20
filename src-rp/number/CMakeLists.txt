# Library: Number and Base Support (obj only)
add_library(numbers INTERFACE)

target_sources(numbers INTERFACE
    nbase.c
    num.c
)

target_include_directories(numbers INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/cmd
)

add_subdirectory(cmd)

target_link_libraries(numbers INTERFACE
    numbers_cmd
    pico_stdlib
)
