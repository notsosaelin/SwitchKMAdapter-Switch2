# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# Target board. Defaults to the Pico 2 W (RP2350).
# Override on the command line to build for the original Pico W:
#   cmake -DPICO_BOARD=pico_w ...
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2_w)
endif()
message(STATUS "Building SwitchKMAdapter for PICO_BOARD=${PICO_BOARD}")

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/bluepad32)

# Use the BTstack bundled with the Pico SDK (matches Bluepad32 v4.2.0's pico_w example,
# which is the known-good recipe for SDK 2.1 + pico2_w). To use Bluepad32's own BTstack
# instead, see bluepad32/examples/pico_w/CMakeLists.txt.
set(BTSTACK_ROOT ${PICO_SDK_PATH}/lib/btstack)

project(SwitchKMAdapter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

file(GLOB_RECURSE SOURCES "src/*.c")
add_executable(SwitchKMAdapter ${SOURCES})

target_include_directories(SwitchKMAdapter PRIVATE
    src
    bluepad32/src/components/bluepad32/include)
include_directories(include)

# Needed for btstack_config.h / sdkconfig.h
# so that libbluepad32 can include them
include_directories(SwitchKMAdapter src)

# Needed when using BTstack (bluedroid headers + IDE include resolution)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/encoder/include)
include_directories(${BTSTACK_ROOT}/3rd-party/bluedroid/decoder/include)
include_directories(${BTSTACK_ROOT}/src)

target_link_libraries(SwitchKMAdapter
    pico_stdlib
    pico_cyw43_arch_none
    pico_btstack_classic
    pico_btstack_cyw43
    bluepad32
    tinyusb_device
    tinyusb_board
    pico_multicore
    hardware_flash
)

add_subdirectory(bluepad32/src/components/bluepad32 libbluepad32)

# USB is used for the Switch HID device (TinyUSB owns it), so no USB stdio.
pico_enable_stdio_usb(SwitchKMAdapter 0)
pico_enable_stdio_uart(SwitchKMAdapter 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(SwitchKMAdapter)
