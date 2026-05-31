# Library: Hardware Runtime for board address 0 (hwrt0) (obj only)
add_library(hwrt INTERFACE)

target_sources(hwrt INTERFACE
    hwrt.c
)

target_link_libraries(hwrt INTERFACE
    app
    pico_multicore
    pico_stdlib
)
