add_executable(gui2c_server_ili9488_pico
    main.c
)

target_include_directories(gui2c_server_ili9488_pico PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/config
)

target_link_libraries(gui2c_server_ili9488_pico
    PRIVATE
        gui2c
        gui2c_platform_pico
        pico_stdlib
        hardware_i2c
        hardware_spi
)

target_compile_definitions(gui2c PRIVATE
    GUI2C_SERVER_TARGET_PROFILE=GUI2C_SERVER_TARGET_PROFILE_ILI9488
)

pico_enable_stdio_usb(gui2c_server_ili9488_pico 1)
pico_enable_stdio_uart(gui2c_server_ili9488_pico 0)

pico_add_extra_outputs(gui2c_server_ili9488_pico)
