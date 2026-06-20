# Library: SD Card operation Commands (shell) - TEMPLATE (Library/Interface)
add_library(disk_cmd INTERFACE)

target_sources(disk_cmd INTERFACE
  cmds.c
)

target_include_directories(disk_cmd INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/app/shell/include
	${CMAKE_CURRENT_LIST_DIR}/../include
)

target_link_libraries(disk_cmd INTERFACE
    cmd
	dskops
)
