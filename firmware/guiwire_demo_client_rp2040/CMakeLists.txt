# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20)

include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake)
project(guiwire_demo_client_rp2040 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../common ${CMAKE_BINARY_DIR}/guiwire_common)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../ports/pico ${CMAKE_BINARY_DIR}/guiwire_port_pico)

add_executable(guiwire_demo_client_rp2040 main.c)
target_include_directories(guiwire_demo_client_rp2040 PRIVATE ${CMAKE_CURRENT_LIST_DIR}/config)
target_compile_definitions(guiwire_demo_client_rp2040 PRIVATE GUIWIRE_DEVICE_QUEUE_CAPACITY=8u)
target_link_libraries(guiwire_demo_client_rp2040 PRIVATE
    guiwire_device_runtime
    guiwire_platform_pico
    pico_stdlib
    hardware_uart
)

pico_enable_stdio_usb(guiwire_demo_client_rp2040 0)
pico_enable_stdio_uart(guiwire_demo_client_rp2040 0)
pico_add_extra_outputs(guiwire_demo_client_rp2040)
