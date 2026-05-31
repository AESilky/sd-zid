# Library: Debug Operations (obj only)
add_library(debugging INTERFACE)

target_sources(debugging INTERFACE
    debug_hw.c
	debug_support.c
)

target_include_directories(debugging INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

add_subdirectory(cmd)

target_link_libraries(debugging INTERFACE
    db_hardfault
    pico_stdlib
)

# Hard-Fault handler
add_library(db_hardfault INTERFACE)
target_sources(db_hardfault INTERFACE db_hardfault.c)
target_link_libraries(db_hardfault INTERFACE pico_stdlib)

