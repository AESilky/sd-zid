# Library: Debug Operations (obj only)
add_library(doc INTERFACE)

target_sources(doc INTERFACE
    dc.c
)

target_include_directories(doc INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

add_subdirectory(cmd)

target_link_libraries(doc INTERFACE
    pico_stdlib
)
