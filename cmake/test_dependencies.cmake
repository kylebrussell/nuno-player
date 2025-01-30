# Unity Configuration
add_library(unity STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/unity/src/unity.c
)

target_include_directories(unity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/unity/src
)

# CMock Configuration
add_library(cmock STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/cmock/src/cmock.c
)

target_include_directories(cmock PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/cmock/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/unity/src
)

# Function to generate mocks using CMock
function(generate_mock HEADER_FILE)
    get_filename_component(HEADER_NAME ${HEADER_FILE} NAME_WE)
    set(MOCK_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_mocks)
    set(MOCK_FILE ${MOCK_OUTPUT_DIR}/mock_${HEADER_NAME}.c)

    # Create output directory
    file(MAKE_DIRECTORY ${MOCK_OUTPUT_DIR})

    # Generate mock using Ruby script
    add_custom_command(
        OUTPUT ${MOCK_FILE}
        COMMAND ruby ${CMAKE_CURRENT_SOURCE_DIR}/external/cmock/lib/cmock.rb 
                    -o${CMAKE_CURRENT_SOURCE_DIR}/config/cmock_config.yml
                    ${HEADER_FILE}
        DEPENDS ${HEADER_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating mock for ${HEADER_FILE}"
    )

    # Create a target for the generated mock
    add_library(mock_${HEADER_NAME} STATIC ${MOCK_FILE})
    target_link_libraries(mock_${HEADER_NAME} PUBLIC cmock)
    target_include_directories(mock_${HEADER_NAME} PUBLIC
        ${MOCK_OUTPUT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endfunction()