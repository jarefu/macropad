cmake_minimum_required(VERSION 3.13)

# Default to Adafruit MacroPad RP2040 board definition (8MB flash)
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD adafruit_macropad_rp2040)
endif()

include(pico_sdk_import.cmake)

project(macropad_native_c C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(macropad
    main.c
    config.c
    hardware.c
    usb_descriptors.c
)

target_compile_definitions(macropad PRIVATE
    PICO_BUILD=1
)

target_include_directories(macropad PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Generate PIO header for WS2812B NeoPixels
pico_generate_pio_header(macropad ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_link_libraries(macropad PRIVATE
    pico_stdlib
    pico_unique_id
    hardware_gpio
    hardware_spi
    hardware_pwm
    hardware_pio
    hardware_clocks
    hardware_flash
    hardware_sync
    hardware_timer
    tinyusb_device
    tinyusb_board
)

# Generate .uf2, .bin, .hex, .map outputs for drag-and-drop flashing via RPI-RP2 bootloader
pico_add_extra_outputs(macropad)

