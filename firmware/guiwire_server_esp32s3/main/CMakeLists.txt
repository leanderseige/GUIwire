# SPDX-License-Identifier: MIT
set(GUIWIRE_COMMON "${CMAKE_CURRENT_LIST_DIR}/../../common")
set(GUIWIRE_ESP_PORT "${CMAKE_CURRENT_LIST_DIR}/../../ports/esp32")

idf_component_register(
    SRCS
        "main.c" "board.c"
        "${GUIWIRE_ESP_PORT}/guiwire_framebuffer.c"
        "${GUIWIRE_COMMON}/src/guiwire_color.c"
        "${GUIWIRE_COMMON}/src/guiwire_link.c"
        "${GUIWIRE_COMMON}/src/guiwire_protocol.c"
        "${GUIWIRE_COMMON}/src/guiwire_status.c"
        "${GUIWIRE_COMMON}/src/guiwire_host.c"
        "${GUIWIRE_COMMON}/src/lib/server/guiwire_server.c"
        "${GUIWIRE_COMMON}/src/lib/server/guiwire_server_objects.c"
        "${GUIWIRE_COMMON}/src/lib/objects/guiwire_object_text.c"
        "${GUIWIRE_COMMON}/src/lib/objects/guiwire_object_rect.c"
        "${GUIWIRE_COMMON}/src/lib/objects/guiwire_object_line.c"
        "${GUIWIRE_COMMON}/src/lib/objects/guiwire_object_bar.c"
        "${GUIWIRE_COMMON}/src/lib/objects/guiwire_object_ring.c"
        "${GUIWIRE_COMMON}/src/lib/objects/guiwire_object_waveform.c"
        "${GUIWIRE_COMMON}/src/lib/renderer/guiwire_font_5x7.c"
        "${GUIWIRE_COMMON}/src/lib/renderer/guiwire_renderer_text.c"
    INCLUDE_DIRS
        "." "${GUIWIRE_COMMON}/include" "${GUIWIRE_ESP_PORT}/include"
    PRIV_INCLUDE_DIRS "${GUIWIRE_COMMON}/src"
    REQUIRES esp_driver_uart esp_driver_gpio esp_driver_i2c esp_driver_spi
             esp_lcd esp_timer esp_system esp_rom espressif__esp_lcd_sh8601
)
target_link_libraries(${COMPONENT_LIB} PUBLIC m)
