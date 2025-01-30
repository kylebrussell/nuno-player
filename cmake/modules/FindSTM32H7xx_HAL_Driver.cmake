# Dummy STM32 HAL find module for testing
include(FindPackageHandleStandardArgs)

if(USE_MOCK_HAL)
    add_library(STM32H7xx_HAL_Mock INTERFACE)
    add_library(STM32H7xx_HAL_Driver ALIAS STM32H7xx_HAL_Mock)
else()
    add_library(STM32H7xx_HAL_Driver INTERFACE)
    target_include_directories(STM32H7xx_HAL_Driver INTERFACE 
        ${CMAKE_CURRENT_SOURCE_DIR}/external/STM32H7xx_HAL_Driver/Inc
    )
endif()

find_package_handle_standard_args(STM32H7xx_HAL_Driver DEFAULT_MSG)