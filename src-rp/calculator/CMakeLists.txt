# Library: Debug Operations (obj only)
add_library(calculator INTERFACE)

add_subdirectory(cmd)

target_sources(calculator INTERFACE
    calc.c
)

target_include_directories(calculator INTERFACE
	${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(calculator INTERFACE
    numbers
    pico_stdlib
)
