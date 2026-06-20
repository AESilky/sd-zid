# Library: Regular Expressions tiny (obj only)
add_library(regext INTERFACE)

target_sources(regext INTERFACE
    re.c
)

target_include_directories(regext INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(regext INTERFACE
    pico_stdlib
)
