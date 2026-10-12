# SPDX-License-Identifier: MIT

add_executable(guiwire_server_st7789_pico
    main.c
    ../server_common/guiwire_server_pico_app.c
)

option(GUIWIRE_SERVER_DEBUG_DISPLAY "Show the diagnostic screen instead of the GUI" ON)
if (GUIWIRE_SERVER_DEBUG_DISPLAY)
    target_compile_definitions(guiwire_server_st7789_pico PRIVATE GUIWIRE_SERVER_DEBUG_DISPLAY=1)
endif()
target_compile_definitions(guiwire_server_st7789_pico PRIVATE
    GUIWIRE_UART_BAUD=${GUIWIRE_UART_BAUD}
)
if (GUIWIRE_ENABLE_BAUD_NEGOTIATION)
    target_compile_definitions(guiwire_server_st7789_pico PRIVATE
        GUIWIRE_ENABLE_BAUD_NEGOTIATION=1
    )
endif()

target_include_directories(guiwire_server_st7789_pico PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${CMAKE_CURRENT_LIST_DIR}/../server_common
    ${CMAKE_CURRENT_LIST_DIR}/../../../common/src
)
target_link_libraries(guiwire_server_st7789_pico PRIVATE
    guiwire_server_ili9488
    guiwire_display_st7789
    guiwire_platform_pico
    pico_multicore
    pico_rand
    pico_stdlib
    hardware_spi
    hardware_uart
)
pico_enable_stdio_usb(guiwire_server_st7789_pico 0)
pico_enable_stdio_uart(guiwire_server_st7789_pico 0)
pico_add_extra_outputs(guiwire_server_st7789_pico)
