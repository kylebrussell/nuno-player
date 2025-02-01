cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake)

# Add modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

project(nuno-player C)

# Compiler flags
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTS "Build test suite" ON)
option(USE_MOCK_HAL "Use mock HAL implementation" ON)
option(BUILD_SIM "Build simulation target with SDL" ON)

# Dependencies for embedded target
find_package(CMSIS REQUIRED)
find_package(STM32H7xx_HAL_Driver REQUIRED)
find_package(FreeRTOS REQUIRED)

# For simulation, add SDL2 dependency (installed via brew)
if(BUILD_SIM)
    find_package(SDL2 REQUIRED)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# -------------------------------------------------------------------
# Libraries and Executable Targets
# -------------------------------------------------------------------

# Drivers library
add_library(drivers
    src/drivers/es9038q2m/es9038q2m.c
    # Add other driver source files as they are created
)

# Core library (e.g., audio pipeline, UI, etc.)
add_library(core
    src/core/audio/audio_pipeline.c
    # Add other core components as they are created
)

# Platform library (I2C, GPIO, DMA, etc.)
add_library(platform
    src/platform/i2c.c
    src/platform/gpio.c
    src/platform/dma.c
    # Add other platform source files as needed
)

# Main executable for the embedded target
add_executable(nuno-player
    src/main.c
)

target_link_libraries(nuno-player
    drivers
    core
    platform
    freertos_kernel
)

if(USE_MOCK_HAL)
    target_link_libraries(nuno-player STM32H7xx_HAL_Mock)
else()
    target_link_libraries(nuno-player STM32H7xx_HAL_Driver)
endif()

# Simulation executable using SDL mock display (for UI iteration without hardware)
if(BUILD_SIM)
    add_executable(nuno-sim
         src/platform/sim/main_ui_test.c
         src/platform/sim/sdl_mock_display.c
    )

    target_include_directories(nuno-sim PRIVATE
         ${CMAKE_CURRENT_SOURCE_DIR}/include
         ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(nuno-sim
         drivers
         core
         platform
         freertos_kernel
         SDL2::SDL2
    )
endif()

# -------------------------------------------------------------------
# Test Configuration
# -------------------------------------------------------------------
if(BUILD_TESTS)
    # Include test dependencies (assumes cmake/test_dependencies.cmake exists)
    include(cmake/test_dependencies.cmake)
    
    # Generate mocks for the platform interface
    generate_mock(${CMAKE_CURRENT_SOURCE_DIR}/include/nuno/platform.h)
    
    # Add mock libraries for testing
    add_library(platform_mock
        tests/mocks/mock_platform.c
        tests/mocks/stm32h7xx_hal_mock.c
    )

    target_link_libraries(platform_mock PUBLIC
        unity
        cmock
        mock_platform
    )

    target_include_directories(platform_mock PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks
        ${CMAKE_CURRENT_SOURCE_DIR}/external/unity/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/cmock/src
    )

    # ES9038Q2M DAC Tests executable
    add_executable(es9038q2m_tests
        tests/drivers/es9038q2m_tests.c
    )

    target_link_libraries(es9038q2m_tests
        platform_mock
        unity
        cmock
        drivers
    )

    # Platform Tests executable
    add_executable(platform_tests
        tests/platform/platform_tests.c
    )

    target_link_libraries(platform_tests
        platform_mock
        unity
    )

    # Register tests with CTest
    add_test(NAME ES9038Q2M_Tests COMMAND es9038q2m_tests)
    add_test(NAME Platform_Tests COMMAND platform_tests)

    # Test-specific include directories
    target_include_directories(es9038q2m_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/es9038q2m
    )

    target_include_directories(platform_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    )
endif()

# -------------------------------------------------------------------
# Installation
# -------------------------------------------------------------------
install(TARGETS nuno-player
    RUNTIME DESTINATION bin
)

if(BUILD_TESTS)
    install(TARGETS es9038q2m_tests platform_tests
        RUNTIME DESTINATION bin/tests
    )
endif()