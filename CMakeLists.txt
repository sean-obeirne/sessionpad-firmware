cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(sessionpad C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(sessionpad main.c)

target_link_libraries(sessionpad pico_stdlib)

# Route stdio to USB CDC (serial over USB), disable UART
pico_enable_stdio_usb(sessionpad 1)
pico_enable_stdio_uart(sessionpad 0)

pico_add_extra_outputs(sessionpad)
