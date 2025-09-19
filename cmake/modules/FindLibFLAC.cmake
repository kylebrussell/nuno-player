# LibFLAC find module
include(FindPackageHandleStandardArgs)

get_filename_component(NUNO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(FLAC_SOURCE_DIR "${NUNO_ROOT}/external/libflac")

add_library(libflac STATIC
    ${FLAC_SOURCE_DIR}/src/libFLAC/bitmath.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/bitreader.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/bitwriter.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/cpu.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/crc.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/fixed.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/float.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/format.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/lpc.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/md5.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/memory.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/metadata_iterators.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/metadata_object.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/stream_decoder.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/stream_encoder.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/stream_encoder_framing.c
    ${FLAC_SOURCE_DIR}/src/libFLAC/window.c
)

target_compile_definitions(libflac PRIVATE
    FLAC__NO_DLL
    HAVE_CONFIG_H
    FLAC__NO_ASM
    PACKAGE_VERSION="1.4.3"
)

target_include_directories(libflac PUBLIC
    ${FLAC_SOURCE_DIR}/include
    ${FLAC_SOURCE_DIR}/src/libFLAC/include
)

add_library(LibFLAC::FLAC ALIAS libflac)

find_package_handle_standard_args(LibFLAC DEFAULT_MSG)
