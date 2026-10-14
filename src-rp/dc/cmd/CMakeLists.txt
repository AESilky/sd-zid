# Library: Debug Operations (obj only)
add_library(dc_cmd INTERFACE)

target_sources(dc_cmd INTERFACE
    cmds.c
)

target_include_directories(dc_cmd INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/..
	${CMAKE_CURRENT_LIST_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/Csmap/src
)

target_link_libraries(dc_cmd INTERFACE
	cmd
    csmap
    regext
    pico_stdlib
)
