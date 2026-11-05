# Build the Pico programs.

cmake_minimum_required(VERSION 3.14)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

# Use the Pico tools installed on this computer.

# Keep these settings so the Raspberry Pi Pico VS Code extension works.
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Load the Pico SDK before calling project().
include(pico_sdk_import.cmake)

project(Ultrasonic C CXX ASM)

# Set up the Pico SDK.
pico_sdk_init()

# Build the DSP math library using the CMSIS headers from the Pico SDK.
set(CMSISCORE "${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core")
add_subdirectory(external/CMSIS-DSP)
# Only build the DSP source groups this project needs. Building the whole
# library makes GCC 15.2 crash in arm_abs_q31, a function we do not use.
get_target_property(dsp_sources CMSISDSP SOURCES)
list(FILTER dsp_sources INCLUDE REGEX "(TransformFunctions/.*(f32)\\.c|TransformFunctions/arm_bitreversal[2]?\\.c|CommonTables/.*\\.c|FilteringFunctions/arm_correlate_f32\\.c|SupportFunctions/arm_(copy|fill)_.*\\.c)$")
set_property(TARGET CMSISDSP PROPERTY SOURCES "${dsp_sources}")

add_library(correlation STATIC src/correlation.c)
target_include_directories(correlation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(correlation PUBLIC CMSISDSP)

add_executable(ultrasonic src/main.c src/acquisition.c)
add_executable(ultrasonic_benchmark benchmarks/benchmark.c)
foreach(target ultrasonic ultrasonic_benchmark)
    target_link_libraries(${target} PRIVATE correlation pico_stdlib)
    pico_set_program_name(${target} "${target}")
    pico_set_program_version(${target} "0.1")
    pico_enable_stdio_uart(${target} 0)
    pico_enable_stdio_usb(${target} 1)
    target_compile_definitions(${target} PRIVATE PICO_STDIO_USB_CONNECTION_WITHOUT_DTR=1)
    pico_add_extra_outputs(${target})
endforeach()
