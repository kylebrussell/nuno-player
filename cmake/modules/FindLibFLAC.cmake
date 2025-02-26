# LibFLAC find module
include(FindPackageHandleStandardArgs)

# Create the main libFLAC library
add_library(libflac STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/bitmath.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/bitreader.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/bitwriter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/cpu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/crc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/fixed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/float.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/format.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/lpc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/md5.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/memory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/metadata_iterators.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/metadata_object.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/stream_decoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/stream_encoder.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/stream_encoder_framing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/window.c
)

# Define build options
target_compile_definitions(libflac PRIVATE
    FLAC__NO_DLL
    HAVE_CONFIG_H
    FLAC__NO_ASM
)

target_include_directories(libflac PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/libflac/src/libFLAC/include
)

# Create an alias target with namespace
add_library(LibFLAC::FLAC ALIAS libflac)

find_package_handle_standard_args(LibFLAC DEFAULT_MSG) 