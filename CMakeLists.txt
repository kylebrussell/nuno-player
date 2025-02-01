cmake_minimum_required(VERSION 3.20)

# Options
option(BUILD_TESTS "Build test suite" ON)
option(USE_MOCK_HAL "Use mock HAL implementation" ON)
option(BUILD_SIM "Build simulation target with SDL" ON)

# For simulation builds, ensure no ARM toolchain is used.
if(BUILD_SIM)
  if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR "BUILD_SIM is ON, but CMAKE_TOOLCHAIN_FILE is set to '${CMAKE_TOOLCHAIN_FILE}'. Delete your build directory or re-run CMake without specifying a toolchain file.")
  else()
    message(STATUS "BUILD_SIM is ON. Using host compiler.")
  endif()
else()
  # For embedded builds, set the ARM toolchain.
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake")
endif()

project(nuno-player C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

if(BUILD_SIM)
  find_package(SDL2 REQUIRED)
endif()

if(NOT BUILD_SIM)
  find_package(CMSIS REQUIRED)
  find_package(STM32H7xx_HAL_Driver REQUIRED)
  find_package(FreeRTOS REQUIRED)
endif()

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Libraries
add_library(drivers
    src/drivers/es9038q2m/es9038q2m.c
)

# No core sources yet—make it an INTERFACE target.
add_library(core INTERFACE)

add_library(platform
    src/platform/i2c.c
    src/platform/gpio.c
    src/platform/dma.c
)

# Executable Targets
if(NOT BUILD_SIM)
  add_executable(nuno-player
      src/platform/main.c
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
endif()

if(BUILD_SIM)
  add_executable(nuno-sim
      src/platform/sim/main_ui_test.c
      src/platform/sim/sdl_mock_display.c
  )
  target_include_directories(nuno-sim PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  target_link_libraries(nuno-sim
      drivers
      core
      platform
      freertos_kernel
      SDL2::SDL2
  )
endif()

if(BUILD_TESTS)
  include(cmake/test_dependencies.cmake)
  generate_mock("${CMAKE_CURRENT_SOURCE_DIR}/include/nuno/platform.h")
  
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
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks"
      "${CMAKE_CURRENT_SOURCE_DIR}/external/unity/src"
      "${CMAKE_CURRENT_SOURCE_DIR}/external/cmock/src"
  )
  
  add_executable(es9038q2m_tests
      tests/drivers/es9038q2m_tests.c
  )
  target_link_libraries(es9038q2m_tests
      platform_mock
      unity
      cmock
      drivers
  )
  
  add_executable(platform_tests
      tests/platform/platform_tests.c
  )
  target_link_libraries(platform_tests
      platform_mock
      unity
  )
  
  add_test(NAME ES9038Q2M_Tests COMMAND es9038q2m_tests)
  add_test(NAME Platform_Tests COMMAND platform_tests)
  
  target_include_directories(es9038q2m_tests PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/es9038q2m"
  )
  target_include_directories(platform_tests PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src/platform"
  )
endif()

# Installation
if(NOT BUILD_SIM)
  install(TARGETS nuno-player
      RUNTIME DESTINATION bin
  )
else()
  install(TARGETS nuno-sim
      RUNTIME DESTINATION bin
  )
endif()

if(BUILD_TESTS)
  install(TARGETS es9038q2m_tests platform_tests
      RUNTIME DESTINATION bin/tests
  )
endif()