# Library: Application (APP) functionality (obj only)
add_library(app INTERFACE)

target_sources(app INTERFACE
  app.c
  appops.c
)

target_include_directories(app INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_SOURCE_DIR}/lib/shell/src/include
)

target_link_libraries(app INTERFACE
  dskops
  shell
  #shell_stub
  pico_stdlib
)
