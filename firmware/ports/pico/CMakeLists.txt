# SPDX-License-Identifier: MIT

add_library(guiwire_platform_pico STATIC src/guiwire_platform_pico.c)
target_include_directories(guiwire_platform_pico
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_CURRENT_LIST_DIR}/../../common/include
)
target_link_libraries(guiwire_platform_pico
    PUBLIC
        pico_stdlib
        hardware_uart
        hardware_spi
        m
)

add_library(guiwire_display_st7789 STATIC displays/guiwire_display_st7789.c)
target_include_directories(guiwire_display_st7789
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/displays/include
        ${CMAKE_CURRENT_LIST_DIR}/../../common/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../../common/src
)
target_link_libraries(guiwire_display_st7789 PUBLIC guiwire_core m)
