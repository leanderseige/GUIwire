# SPDX-License-Identifier: MIT

add_executable(guiwire_server_ili9488_pico
    main.c
    ../server_common/guiwire_server_pico_app.c
)

target_include_directories(guiwire_server_ili9488_pico PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${CMAKE_CURRENT_LIST_DIR}/../server_common
)
target_compile_definitions(guiwire_server_ili9488_pico PRIVATE
    GUIWIRE_UART_BAUD=${GUIWIRE_UART_BAUD}
)
if (GUIWIRE_ENABLE_BAUD_NEGOTIATION)
    target_compile_definitions(guiwire_server_ili9488_pico PRIVATE
        GUIWIRE_ENABLE_BAUD_NEGOTIATION=1
    )
endif()

target_link_libraries(guiwire_server_ili9488_pico PRIVATE
    guiwire_server_ili9488
    guiwire_display_ili9488
    guiwire_platform_pico
    pico_multicore
    pico_stdlib
    hardware_spi
    hardware_uart
)

pico_enable_stdio_usb(guiwire_server_ili9488_pico 0)
pico_enable_stdio_uart(guiwire_server_ili9488_pico 0)
pico_add_extra_outputs(guiwire_server_ili9488_pico)
