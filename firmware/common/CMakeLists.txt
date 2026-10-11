# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(guiwire C)
endif()

set(CMAKE_C_STANDARD 11)
include(GNUInstallDirs)

add_library(guiwire_core STATIC
    src/guiwire_color.c
    src/guiwire_link.c
    src/guiwire_protocol.c
    src/guiwire_status.c
)

target_include_directories(guiwire_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(guiwire_core PUBLIC m)

add_library(guiwire_device STATIC src/guiwire_device.c)
target_include_directories(guiwire_device
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src
)
target_link_libraries(guiwire_device PUBLIC guiwire_core)

set(GUIWIRE_HOST_SOURCES
    src/guiwire_host.c
    src/lib/server/guiwire_server.c
    src/lib/server/guiwire_server_objects.c
    src/lib/objects/guiwire_object_text.c
    src/lib/objects/guiwire_object_rect.c
    src/lib/objects/guiwire_object_line.c
    src/lib/objects/guiwire_object_bar.c
    src/lib/objects/guiwire_object_ring.c
    src/lib/objects/guiwire_object_waveform.c
    src/lib/renderer/guiwire_font_5x7.c
    src/lib/renderer/guiwire_renderer_text.c
)

add_library(guiwire_host STATIC ${GUIWIRE_HOST_SOURCES})
target_include_directories(guiwire_host
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src
)
target_link_libraries(guiwire_host PUBLIC guiwire_core m)

add_library(guiwire_device_runtime STATIC src/guiwire_device_runtime.c)
target_include_directories(guiwire_device_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src
)
target_link_libraries(guiwire_device_runtime PUBLIC guiwire_device guiwire_core)

add_library(guiwire INTERFACE)
target_link_libraries(guiwire INTERFACE
    guiwire_core
    guiwire_device
    guiwire_device_runtime
    guiwire_host
)
add_library(guiwire::guiwire ALIAS guiwire)
add_library(guiwire::core ALIAS guiwire_core)
add_library(guiwire::device ALIAS guiwire_device)
add_library(guiwire::device_runtime ALIAS guiwire_device_runtime)
add_library(guiwire::host ALIAS guiwire_host)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS guiwire guiwire_core guiwire_device guiwire_device_runtime guiwire_host
        EXPORT guiwireTargets)
install(EXPORT guiwireTargets
        NAMESPACE guiwire::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/guiwire)
