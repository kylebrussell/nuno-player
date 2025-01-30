# FreeRTOS find module for testing
include(FindPackageHandleStandardArgs)

# Create the main FreeRTOS library
add_library(freertos_kernel INTERFACE)
target_include_directories(freertos_kernel INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/external/FreeRTOS/include
)

# Create an alias target with namespace
add_library(FreeRTOS::Kernel ALIAS freertos_kernel)

find_package_handle_standard_args(FreeRTOS DEFAULT_MSG)