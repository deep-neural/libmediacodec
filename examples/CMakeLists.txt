cmake_minimum_required(VERSION 3.10)
project(WebTransportExamples)

# Path to the parent directory containing libs and headers
set(BASE_DIR ${CMAKE_SOURCE_DIR}/..)

# Include directory
include_directories(${BASE_DIR})

# Link directory
link_directories(
    ${BASE_DIR}
)

add_executable(vp8_encoder vp8_encoder.cc)

add_executable(vp9_encoder vp9_encoder.cc)

add_executable(h264_encoder h264_encoder.cc)

add_executable(nvidia_h264_encoder nvidia_h264_encoder.cc)

add_executable(nvidia_av1_encoder nvidia_av1_encoder.cc)

add_executable(nvidia_hevc_encoder nvidia_hevc_encoder.cc)

set(EXAMPLES_TARGETS
    vp8_encoder
    vp9_encoder
    h264_encoder
    nvidia_h264_encoder
    nvidia_av1_encoder
    nvidia_hevc_encoder
)

foreach(TARGET ${EXAMPLES_TARGETS})
    set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 20)
    # Link with the webtransport library
    target_link_libraries(${TARGET} mediacodec)
    
endforeach()