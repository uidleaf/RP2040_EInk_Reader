cmake_minimum_required(VERSION 3.13)

# ============================================================
# RP2040 2.9" E-Ink Reader — Pico SDK build
# ============================================================
# Set PICO_SDK_PATH in your environment, or pass it to cmake:
#   mkdir build && cd build
#   cmake -DPICO_SDK_PATH=/path/to/pico-sdk ..
#   make -j4
# ============================================================

# Pull in the Pico SDK (must be before project())
include(pico_sdk_import.cmake)

project(eink_reader C CXX ASM)

# Workaround: prevent cmake 4.3 FindDoxygen from crashing on Chinese paths
set(DOXYGEN_FOUND FALSE CACHE BOOL "Skip Doxygen" FORCE)

# Initialise the SDK
pico_sdk_init()

# Add FatFS library
add_subdirectory(lib/pico-fatfs/src)

# --- Executable ---
add_executable(eink_reader
    main.c
    DEV_Config.c
    EPD_2in9.c
    ui.c
    hw_config.c
    font_gb2312.c
    msc_disk.c
    usb_descriptors.c
)

# --- Link libraries ---
target_link_libraries(eink_reader PRIVATE
    pico_stdlib
    hardware_spi
    hardware_gpio
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
    tinyusb_board
    tinyusb_device
)

target_include_directories(eink_reader PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# --- Enable USB stdio so printf goes to the serial port ---
pico_enable_stdio_usb(eink_reader 1)
pico_enable_stdio_uart(eink_reader 0)

# --- Generate .uf2 / .elf / .bin ---
pico_add_extra_outputs(eink_reader)

# --- Optional: raise warning level ---
target_compile_options(eink_reader PRIVATE -Wall -Wextra)
