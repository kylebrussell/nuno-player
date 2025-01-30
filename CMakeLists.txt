cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake)

# Add modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

project(nuno-player C)

# Compiler flags
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Option for testing
option(BUILD_TESTS "Build test suite" ON)
option(USE_MOCK_HAL "Use mock HAL implementation" ON)

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

# Main executable
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

# Test Configuration
if(BUILD_TESTS)
    # Add test dependencies
    include(cmake/test_dependencies.cmake)
    
    # Generate mocks for platform interface
    generate_mock(${CMAKE_CURRENT_SOURCE_DIR}/include/nuno/platform.h)
    
    # Add mock libraries
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

    # ES9038Q2M DAC Tests
    add_executable(es9038q2m_tests
        tests/drivers/es9038q2m_tests.c
    )

    target_link_libraries(es9038q2m_tests
        platform_mock
        unity
        cmock
        drivers
    )

    # Platform Tests
    add_executable(platform_tests
        tests/platform/platform_tests.c
    )

    target_link_libraries(platform_tests
        platform_mock
        unity
    )

    # Register tests
    add_test(NAME ES9038Q2M_Tests COMMAND es9038q2m_tests)
    add_test(NAME Platform_Tests COMMAND platform_tests)

    # Test include directories
    target_include_directories(es9038q2m_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/es9038q2m
    )

    target_include_directories(platform_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    )
endif()

# Installation
install(TARGETS nuno-player
    RUNTIME DESTINATION bin
)

if(BUILD_TESTS)
    install(TARGETS es9038q2m_tests platform_tests
        RUNTIME DESTINATION bin/tests
    )
endif()