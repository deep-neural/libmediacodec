cmake_minimum_required(VERSION 3.10)
project(mediacodec)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set vcpkg toolchain file
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

# Detect platform and set vcpkg triplet
if(WIN32)
    set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "")
    # Use appropriate linker flag for Windows
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /FORCE:MULTIPLE")
    # Add Visual Studio compiler flags to fix C++14 compatibility issues
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
    
    # Ensure DLL and LIB files are generated on Windows
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()  # Assume Unix (Linux)
    set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "")
    # Add -Wl,-Bsymbolic flag for Unix-like systems
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-Bsymbolic")
endif()

# Set vcpkg installed directory
set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VCPKG_INSTALLED_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/accelerated
)
link_directories(
    ${VCPKG_INSTALLED_DIR}/lib
)

# Set CMAKE_PREFIX_PATH
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${VCPKG_INSTALLED_DIR}")

# Find static FFmpeg libraries
if(WIN32)
    # Windows static libraries typically have .lib extension
    find_library(AVCODEC_LIB avcodec.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(AVDEVICE_LIB avdevice.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(AVFORMAT_LIB avformat.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(AVFILTER_LIB avfilter.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(SWRESAMPLE_LIB swresample.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(SWSCALE_LIB swscale.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(AVUTIL_LIB avutil.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    
    # Find additional codec libraries for Windows
    find_library(LIBVPX_LIBRARY vpx.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(LIBOPUS_LIBRARY opus.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(LIBAOM_LIBRARY aom.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(LIBX264_LIBRARY libx264.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(LIBX265_LIBRARY libx265.lib PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)

else()
    # Unix static libraries have .a extension
    find_library(AVCODEC_LIB libavcodec.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(AVDEVICE_LIB libavdevice.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(AVFORMAT_LIB libavformat.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(AVFILTER_LIB libavfilter.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(SWRESAMPLE_LIB libswresample.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(SWSCALE_LIB libswscale.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(AVUTIL_LIB libavutil.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    
    # Find additional codec libraries for Unix
    find_library(LIBVPX_LIBRARY libvpx.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(LIBOPUS_LIBRARY libopus.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(LIBAOM_LIBRARY libaom.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(LIBX264_LIBRARY libx264.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
    find_library(LIBX265_LIBRARY libx265.a PATHS ${VCPKG_INSTALLED_DIR}/lib REQUIRED)
endif()

# Create shared library
add_library(mediacodec SHARED
    vp9_encoder.cc
    vp9_encoder.h
    vp9_decoder.cc
    vp9_decoder.h

    vp8_encoder.cc
    vp8_encoder.h
    vp8_decoder.cc
    vp8_decoder.h

    opus_encoder.cc
    opus_encoder.h
    opus_decoder.cc
    opus_decoder.h

    hevc_encoder.cc
    hevc_encoder.h
    hevc_decoder.cc
    hevc_decoder.h

    h264_encoder.cc
    h264_encoder.h
    h264_decoder.cc
    h264_decoder.h

    av1_encoder.cc
    av1_encoder.h
    av1_decoder.cc
    av1_decoder.h

    accelerated/nvidia_h264_encoder.cc
    accelerated/nvidia_h264_encoder.h

    accelerated/nvidia_hevc_encoder.cc
    accelerated/nvidia_hevc_encoder.h

    accelerated/nvidia_av1_encoder.cc
    accelerated/nvidia_av1_encoder.h

    media_image_utils.cc
    media_image_utils.h

    media_video_encoder.cc
    media_video_encoder.h
)

# Mark headers as PUBLIC_HEADER for installation
set_target_properties(mediacodec PROPERTIES
    PUBLIC_HEADER "vp9_encoder.h;vp9_decoder.h;vp8_encoder.h;vp8_decoder.h;opus_encoder.h;opus_decoder.h;hevc_encoder.h;hevc_decoder.h;h264_encoder.h;h264_decoder.h;av1_encoder.h;av1_decoder.h;media_video_encoder.h"
)

# Include directory for header files
target_include_directories(mediacodec PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VCPKG_INSTALLED_DIR}/include
)

# Link libraries to the target
target_link_libraries(mediacodec PRIVATE
    ${AVCODEC_LIB}
    ${AVDEVICE_LIB}
    ${AVFORMAT_LIB}
    ${AVFILTER_LIB}
    ${SWRESAMPLE_LIB}
    ${SWSCALE_LIB}
    ${AVUTIL_LIB}
    ${LIBVPX_LIBRARY}
    ${LIBOPUS_LIBRARY}
    ${LIBAOM_LIBRARY}
    ${LIBX264_LIBRARY}
    ${LIBX265_LIBRARY}
)

# FFmpeg often requires additional system libraries
if(UNIX)
    # Common dependencies for FFmpeg on Linux
    target_link_libraries(mediacodec PRIVATE
        z       # zlib
        m       # math library
        pthread # threading
        dl      # dynamic loading
    )
endif()

# Set output name and directories for different platforms
if(WIN32)
    set_target_properties(mediacodec PROPERTIES
        OUTPUT_NAME "mediacodec"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
else()
    set_target_properties(mediacodec PROPERTIES
        OUTPUT_NAME "mediacodec"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
endif()

# Install library and headers
install(TARGETS mediacodec
    RUNTIME DESTINATION bin    # For Windows DLLs
    LIBRARY DESTINATION lib    # For Unix .so files
    ARCHIVE DESTINATION lib    # For Windows .lib files
    PUBLIC_HEADER DESTINATION include
)

# Only build examples after the main library is built
add_subdirectory(examples)