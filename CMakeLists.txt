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

# Audio core is only required for embedded builds.
if(NOT BUILD_SIM)
  find_package(LibFLAC REQUIRED)
endif()

# Libraries
add_library(drivers
    src/drivers/es9038q2m/es9038q2m.c
)
target_include_directories(drivers PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_library(music_catalog
    src/core/audio/music_catalog.c
)
target_include_directories(music_catalog PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_library(core_audio
    src/core/audio/audio_pipeline.c
    src/core/audio/audio_buffer.c
    src/core/audio/music_library.c
    src/core/audio/format_decoder.c
)
target_include_directories(core_audio PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/minimp3"
)
target_compile_definitions(core_audio PUBLIC MINIMP3_IMPLEMENTATION)
target_link_libraries(core_audio PUBLIC
    music_catalog
    drivers
)
if(NOT BUILD_SIM)
  target_link_libraries(core_audio PUBLIC
      LibFLAC::FLAC
  )
endif()

add_library(core_ui
    src/core/ui/menu_renderer.c
    src/core/ui/menu_items.c
    src/core/ui/ui_state.c
    src/core/ui/ui_tasks.c
)
target_include_directories(core_ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/ui"
)
target_link_libraries(core_ui PUBLIC music_catalog)

# Executable Targets
if(NOT BUILD_SIM)
  add_library(platform
      src/platform/i2c.c
      src/platform/gpio.c
      src/platform/dma.c
  )
  target_include_directories(platform PUBLIC
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
  )

  add_executable(nuno-player
      src/platform/main.c
  )
  target_link_libraries(nuno-player
      drivers
      core_audio
      core_ui
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
  add_compile_definitions(BUILD_SIM)
  # Point default library path to absolute assets dir at build time for sim
  target_compile_definitions(core_audio PUBLIC NUNO_DEFAULT_LIBRARY_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/music")
  target_compile_definitions(music_catalog PUBLIC NUNO_DEFAULT_LIBRARY_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/music")
  add_executable(nuno-sim
      src/platform/sim/main_ui_test.c
      src/platform/sim/sdl_mock_display.c
      src/platform/sim/platform_sim.c
      src/platform/sim/audio_controller.c
      src/platform/sim/filesystem_sim.c
  )
  target_include_directories(nuno-sim PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/core/ui"
  )
  target_link_libraries(nuno-sim
      core_audio
      core_ui
      music_catalog
      drivers
      SDL2::SDL2
  )
  if (TARGET SDL2::SDL2main)
    target_link_libraries(nuno-sim SDL2::SDL2main)
  endif()
  if (TARGET SDL2::SDL2)
    get_target_property(_sdl2_includes SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
    if (_sdl2_includes)
      set(_sdl2_parent_dirs "")
      foreach(_dir IN LISTS _sdl2_includes)
        get_filename_component(_parent "${_dir}" DIRECTORY)
        list(APPEND _sdl2_parent_dirs ${_parent})
      endforeach()
      list(REMOVE_DUPLICATES _sdl2_parent_dirs)
      target_include_directories(nuno-sim PRIVATE ${_sdl2_parent_dirs})
    endif()
  endif()
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
