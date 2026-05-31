# Library: cmt ; Cooperative Multi-Tasking
add_library(cmt INTERFACE)

target_sources(cmt INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/cmt.c
  ${CMAKE_CURRENT_LIST_DIR}/cmt_heap.c
)

target_include_directories(cmt INTERFACE
  ${PICO_SDK_PATH}/src/rp2_common/hardware_pwm/include
)

target_link_libraries(cmt INTERFACE
  picoutil
  pico_float
  pico_stdlib
)
