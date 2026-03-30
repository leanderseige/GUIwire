add_executable(gui2c_client_object_demo_pico
    main.c
)

target_include_directories(gui2c_client_object_demo_pico PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/config
)

target_link_libraries(gui2c_client_object_demo_pico
    PRIVATE
        gui2c
        gui2c_platform_pico
        pico_stdlib
        hardware_i2c
)

pico_enable_stdio_usb(gui2c_client_object_demo_pico 1)
pico_enable_stdio_uart(gui2c_client_object_demo_pico 0)

pico_add_extra_outputs(gui2c_client_object_demo_pico)
