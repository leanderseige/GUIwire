# SPDX-License-Identifier: MIT

function(guiwire_add_ssd1306_firmware target height display_target)
    add_executable(${target}
        main.c
        ../server_common/guiwire_server_pico_app.c
    )

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/config
        ${CMAKE_CURRENT_LIST_DIR}/../server_common
    )
    target_compile_definitions(${target} PRIVATE GUIWIRE_SSD1306_HEIGHT=${height}u)
    target_compile_definitions(${target} PRIVATE GUIWIRE_UART_BAUD=${GUIWIRE_UART_BAUD})
    if (GUIWIRE_ENABLE_BAUD_NEGOTIATION)
        target_compile_definitions(${target} PRIVATE GUIWIRE_ENABLE_BAUD_NEGOTIATION=1)
    endif()
    target_link_libraries(${target} PRIVATE
        guiwire_server_ssd1306
        ${display_target}
        guiwire_platform_pico
        pico_multicore
        pico_stdlib
        hardware_spi
        hardware_uart
    )
    pico_enable_stdio_usb(${target} 0)
    pico_enable_stdio_uart(${target} 0)
    pico_add_extra_outputs(${target})
endfunction()

guiwire_add_ssd1306_firmware(guiwire_server_ssd1306_128x32_pico 32 guiwire_display_ssd1306_128x32)
guiwire_add_ssd1306_firmware(guiwire_server_ssd1306_128x64_pico 64 guiwire_display_ssd1306_128x64)
