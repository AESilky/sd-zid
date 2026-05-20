# SilkyDESIGN Shell Test.
cmake_minimum_required(VERSION 3.13)

#
# Add executable (output) for testing
#
add_executable(sl_shell_test
  shell_test.c
  util.c
)

message(STATUS "In test, PICO_SDK_PATH is: ${PICO_SDK_PATH}")

target_include_directories(sl_shell_test BEFORE PUBLIC
  ${CMAKE_SOURCE_DIR}/src/include
  ${CMAKE_SOURCE_DIR}/test
  ${PICO_SDK_PATH}/src/rp2_common/pico_async_context/include
  ${PICO_SDK_PATH}/src/rp2_common/pico_stdio/include
)

#
# Configuration - Debug Mode enabled by default
#
pico_set_program_name(sl_shell_test "SHELL_TEST_DB")
pico_set_program_version(sl_shell_test ${VERSION})
target_compile_definitions(sl_shell_test PUBLIC
	DEBUG_MODE=1
)
# Link in the Modules/Libraries for the top-level
target_link_libraries(sl_shell_test
  shell
  pico_async_context_poll
)

pico_add_extra_outputs(sl_shell_test)
