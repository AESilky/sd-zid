# Library: Pico Utility Commands (shell) - TEMPLATE (Library/Interface)
add_library(pico_cmd INTERFACE)

target_sources(pico_cmd INTERFACE
  cmds.c
)

target_include_directories(pico_cmd INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/shell/include
	${CMAKE_CURRENT_LIST_DIR}/../include
)

target_link_libraries(pico_cmd INTERFACE
    cmd
)
