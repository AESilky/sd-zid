# Library: Numbers and Base Commands (obj only)
add_library(numbers_cmd INTERFACE)

target_sources(numbers_cmd INTERFACE
    cmds.c
)

target_include_directories(numbers_cmd INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(numbers_cmd INTERFACE
	cmd
    numbers
    pico_stdlib
)
