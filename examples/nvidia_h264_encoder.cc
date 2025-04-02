#include "media_video_encoder.h"
#include <iostream>
#include <fstream>
#include <vector>

int main() {
    // Configure encoder for H264 with GPU acceleration
    media::VideoEncoderConfig config;
    config.gpu_acceleration = true;             // Enable GPU-accelerated encoding
    config.output_codec = media::CodecType::H264;
    config.input_format = media::PixelFormat::YUV420; // Set input format to NV12 for GPU acceleration
    config.width = 1920;
    config.height = 1080;
    config.framerate = 30;
    config.bitrate = 5000000;

    // Set H264-specific parameters
    media::codec::H264Params h264_params;
    h264_params.preset = "ultrafast";
    h264_params.profile = "high";
    h264_params.level = "4.1";
    h264_params.keyframe_interval = 120;
    h264_params.max_b_frames = 2;
    h264_params.constant_bitrate = false;
    h264_params.crf = 23;
    h264_params.threads = 0;
    config.SetH264Params(h264_params);

    // Create the H264 encoder instance using the factory method
    auto encoder = media::VideoEncoder::Create(config);
    if (!encoder) {
        std::cerr << "Failed to create GPU-accelerated H264 encoder." << std::endl;
        return -1;
    }

    // Open the NV12 file (assumes "input_nv12.yuv" is in the working directory)
    std::ifstream file("/root/libmediacodec/examples/sample.yuv", std::ios::binary);
    if (!file) {
        std::cerr << "Error opening input_nv12.yuv" << std::endl;
        return -1;
    }

    // Calculate the frame size for NV12 semi-planar format:
    // NV12: Y plane size + interleaved UV plane size (each half the resolution)
    int width = config.width;
    int height = config.height;
    int y_size = width * height;
    int uv_size = (width / 2) * (height / 2) * 2; // two channels interleaved
    int frame_size = y_size + uv_size;

    // Prepare a buffer for one frame
    std::vector<uint8_t> frame_data(frame_size);
    int frame_count = 0;

    // Loop through the file, reading and encoding each frame
    while (file.read(reinterpret_cast<char*>(frame_data.data()), frame_size)) {
        std::vector<uint8_t> encoded_frame;
        if (!encoder->EncodeNV12(frame_data, &encoded_frame)) {
            std::cerr << "Failed to encode frame " << frame_count << std::endl;
            continue;
        }
        std::cout << "Encoded frame " << frame_count 
                  << " size: " << encoded_frame.size() << " bytes" << std::endl;
        frame_count++;
    }

    // Optionally flush any buffered frames
    std::vector<uint8_t> flushed_data;
    if (encoder->Flush(&flushed_data)) {
        std::cout << "Flushed data size: " << flushed_data.size() << " bytes" << std::endl;
    }

    file.close();
    return 0;
}