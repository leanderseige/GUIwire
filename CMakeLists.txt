
cmake_minimum_required(VERSION 3.20)

option(GUI2C_BUILD_PICO_FIRMWARE "Build Pico firmware targets" ON)

if(GUI2C_BUILD_PICO_FIRMWARE)
    include(cmake/pico_sdk_import.cmake)
endif()

project(gui2c_fresh C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_library(gui2c STATIC
    lib/core/gui2c_status.c
    lib/core/gui2c_protocol.c
    lib/client/gui2c_client.c
    lib/server/gui2c_server.c
    lib/server/gui2c_server_objects.c
    lib/objects/gui2c_object_text.c
    lib/objects/gui2c_object_line.c
    lib/objects/gui2c_object_bar.c
    lib/objects/gui2c_object_ring.c
    lib/objects/gui2c_object_waveform.c
    lib/renderer/gui2c_font_5x7.c
    lib/renderer/gui2c_renderer_text.c
    lib/objects/gui2c_object_rect.c    
    lib/display/gui2c_display_ili9488.c
)

target_include_directories(gui2c
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(gui2c
    PUBLIC
        m
)

if(GUI2C_BUILD_PICO_FIRMWARE)
    pico_sdk_init()

    add_library(gui2c_platform_pico STATIC
        platform/pico/gui2c_platform_pico.c
    )

    target_include_directories(gui2c_platform_pico
        PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}/include
    )

    target_link_libraries(gui2c_platform_pico
        PUBLIC
            pico_stdlib
            hardware_i2c
            hardware_spi
            m
    )

    add_subdirectory(firmware/server_ili9488_pico)
    add_subdirectory(firmware/client_object_demo_pico)
    add_subdirectory(firmware/client_brain_module_pico)
endif()
