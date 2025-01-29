cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake)
project(nuno-player C)

# Compiler flags
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Dependencies
find_package(CMSIS REQUIRED)
find_package(STM32H7xx_HAL_Driver REQUIRED)
find_package(FreeRTOS REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Source files
add_library(drivers
    src/drivers/es9038q2m/es9038q2m.c
    # Add other drivers as they're created
)

add_library(core
    src/core/audio/audio_pipeline.c
    # Add other core components as they're created
)

add_library(platform
    src/platform/i2c.c
    src/platform/gpio.c
    src/platform/dma.c
    # Add other platform files as needed
)

option(USE_MOCK_HAL "Use mock HAL implementation" ON)

if(USE_MOCK_HAL)
    add_library(STM32H7xx_HAL_Mock stm32h7xx_hal_mock.c)
    target_include_directories(STM32H7xx_HAL_Mock PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    # Link the mock HAL library
    target_link_libraries(nuno-player STM32H7xx_HAL_Mock)
else()
    # Link the official HAL library
    target_link_libraries(nuno-player STM32H7xx_HAL_Driver)
endif()

# Main executable
add_executable(nuno-player
    src/main.c
)

target_link_libraries(nuno-player
    drivers
    core
    platform
    FreeRTOS::Kernel
    STM32::HAL
)
