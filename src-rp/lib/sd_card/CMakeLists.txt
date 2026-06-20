add_library(SD_FatFs INTERFACE)
target_sources(SD_FatFs INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ffunicode.c
    ${CMAKE_CURRENT_LIST_DIR}/ff15/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/sd_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/hw_config.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/spi.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/sd_card.c
    ${CMAKE_CURRENT_LIST_DIR}/sd_driver/crc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/glue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/f_util.c
    ${CMAKE_CURRENT_LIST_DIR}/src/ff_stdio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/my_debug.c
    ${CMAKE_CURRENT_LIST_DIR}/src/rtc.c
)
target_include_directories(SD_FatFs BEFORE INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ff15/source
    sd_driver
    include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_rtc/include
)
target_link_libraries(SD_FatFs INTERFACE
    hardware_spi
    hardware_dma
    hardware_rtc
    pico_stdlib
)
