# Library: xyz - TEMPLATE (Library/Interface)
add_library(xyz INTERFACE)

target_include_directories(xyz INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/include
)


target_sources(xyz INTERFACE
    xyz.c
)

target_link_libraries(xyz INTERFACE
    pico_stdlib
)
