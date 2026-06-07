# Library: Debug Operations (obj only)
add_library(calculator_cmd INTERFACE)

target_sources(calculator_cmd INTERFACE
    cmds.c
)

target_include_directories(calculator_cmd INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(calculator_cmd INTERFACE
	cmd
    numbers
    pico_stdlib
)
