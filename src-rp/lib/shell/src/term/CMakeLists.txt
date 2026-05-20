# Library: term (terminal) (obj only)
add_library(term INTERFACE)

target_sources(term INTERFACE
    term.c
)

target_link_libraries(term INTERFACE
    pico_stdlib
)
