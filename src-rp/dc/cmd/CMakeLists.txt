# Library: Debug Operations (obj only)
add_library(doc_cmd INTERFACE)

target_sources(doc_cmd INTERFACE
    cmds.c
)

target_include_directories(doc_cmd INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_LIST_DIR}/..
)

target_link_libraries(doc_cmd INTERFACE
	cmd
    pico_stdlib
)
