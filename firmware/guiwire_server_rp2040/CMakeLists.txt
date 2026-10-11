# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20)

option(GUIWIRE_SERVER_BUILD_FIRMWARE "Build GUIwire RP2040 server firmware" ON)
option(GUIWIRE_ENABLE_BAUD_NEGOTIATION "Enable GUIwire runtime baud negotiation" OFF)
set(GUIWIRE_UART_BAUD "9600" CACHE STRING "GUIwire safe startup UART baud rate")

include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)
project(guiwire_server_rp2040 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(GUIWIRE_SERVER_BUILD_FIRMWARE)
    pico_sdk_init()
endif()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../common ${CMAKE_BINARY_DIR}/guiwire_common)

set(GUIWIRE_SERVER_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)
set(GUIWIRE_SERVER_PRIVATE_DIR ${CMAKE_CURRENT_LIST_DIR})

set(GUIWIRE_SERVER_SOURCES
    ../common/src/guiwire_host.c
    ../common/src/lib/server/guiwire_server.c
    ../common/src/lib/server/guiwire_server_objects.c
    ../common/src/lib/objects/guiwire_object_text.c
    ../common/src/lib/objects/guiwire_object_rect.c
    ../common/src/lib/objects/guiwire_object_line.c
    ../common/src/lib/objects/guiwire_object_bar.c
    ../common/src/lib/objects/guiwire_object_ring.c
    ../common/src/lib/objects/guiwire_object_waveform.c
    ../common/src/lib/renderer/guiwire_font_5x7.c
    ../common/src/lib/renderer/guiwire_renderer_text.c
)

function(guiwire_add_server_library target profile)
    add_library(${target} STATIC ${GUIWIRE_SERVER_SOURCES})
    target_include_directories(${target}
        PUBLIC ${GUIWIRE_SERVER_INCLUDE_DIR} ${CMAKE_CURRENT_LIST_DIR}/../common/include
        PRIVATE ${GUIWIRE_SERVER_PRIVATE_DIR} ${CMAKE_CURRENT_LIST_DIR}/../common/src
    )
    target_compile_definitions(${target} PUBLIC GUIWIRE_SERVER_TARGET_PROFILE=${profile})
    target_link_libraries(${target} PUBLIC guiwire_core m)
endfunction()

guiwire_add_server_library(guiwire_server_ili9488 GUIWIRE_SERVER_TARGET_PROFILE_ILI9488)
guiwire_add_server_library(guiwire_server_ssd1306 GUIWIRE_SERVER_TARGET_PROFILE_SSD1306)

add_library(guiwire_display_ili9488 STATIC ../ports/pico/displays/guiwire_display_ili9488.c)
target_include_directories(guiwire_display_ili9488
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../ports/pico/displays/include ${CMAKE_CURRENT_LIST_DIR}/../common/include
    PRIVATE ${GUIWIRE_SERVER_PRIVATE_DIR} ${CMAKE_CURRENT_LIST_DIR}/../common/src
)
target_link_libraries(guiwire_display_ili9488 PUBLIC guiwire_core m)

function(guiwire_add_ssd1306_display target height)
    add_library(${target} STATIC ../ports/pico/displays/guiwire_display_ssd1306.c)
    target_include_directories(${target}
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../ports/pico/displays/include ${CMAKE_CURRENT_LIST_DIR}/../common/include
        PRIVATE ${GUIWIRE_SERVER_PRIVATE_DIR} ${CMAKE_CURRENT_LIST_DIR}/../common/src
    )
    target_compile_definitions(${target} PUBLIC GUIWIRE_SSD1306_HEIGHT=${height}u)
    target_link_libraries(${target} PUBLIC guiwire_core m)
endfunction()

guiwire_add_ssd1306_display(guiwire_display_ssd1306_128x32 32)
guiwire_add_ssd1306_display(guiwire_display_ssd1306_128x64 64)

if(GUIWIRE_SERVER_BUILD_FIRMWARE)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../ports/pico ${CMAKE_BINARY_DIR}/guiwire_port_pico)
    add_subdirectory(firmware/server_ili9488_pico)
    add_subdirectory(firmware/server_ssd1306_pico)
    add_subdirectory(firmware/server_st7789_pico)
endif()
