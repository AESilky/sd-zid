# Library: Interactive Shell with Command Processor, and Terminal (obj only)
add_library(shell INTERFACE)

target_sources(shell INTERFACE
    shell.c
)

add_subdirectory(cmd)
add_subdirectory(term)

target_include_directories(shell INTERFACE
	${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(shell INTERFACE
    cmd
    term
    # Pico Modules/Libraries used
    pico_stdlib
)

# Library: Shell STUB (do nothing shell_modinit and shell_start only)
add_library(shell_stub INTERFACE)

target_sources(shell_stub INTERFACE
    shell_stub.c
)
