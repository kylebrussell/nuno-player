# Dummy CMSIS find module for testing
include(FindPackageHandleStandardArgs)

add_library(CMSIS INTERFACE)
target_include_directories(CMSIS INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/external/CMSIS/Include)

find_package_handle_standard_args(CMSIS DEFAULT_MSG)