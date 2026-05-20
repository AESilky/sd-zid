# Library: Data Bus operation Commands (shell) - TEMPLATE (Library/Interface)
add_library(dbusc_cmd INTERFACE)

target_sources(dbusc_cmd INTERFACE
  cmds.c
)

target_include_directories(dbusc_cmd INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/app/shell/include
	${CMAKE_CURRENT_LIST_DIR}/../include
)

target_link_libraries(dbusc_cmd INTERFACE
    cmd
)
