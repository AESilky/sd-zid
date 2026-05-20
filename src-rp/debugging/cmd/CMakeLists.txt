# Library: Debug Operations (obj only)
add_library(debug_cmd INTERFACE)

target_sources(debug_cmd INTERFACE
    cmds.c
)

target_include_directories(debug_cmd INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(debug_cmd INTERFACE
	cmd
    pico_stdlib
)
