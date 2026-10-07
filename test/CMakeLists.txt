# Program: Z80 Test - Disassembles all Z80 instructions
add_executable(z80datest
    main.c
    z80allbin.c
)

target_include_directories(z80datest PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(z80datest PUBLIC
    z80dasm
)
