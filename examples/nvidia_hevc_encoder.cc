#include "media_video_encoder.h"
#include <iostream>
#include <fstream>
#include <vector>

int main() {
    // Configure encoder for HEVC with GPU acceleration
    media::VideoEncoderConfig config;
    config.gpu_acceleration = true;             // Enable GPU-accelerated encoding
    config.output_codec = media::CodecType::HEVC;
    config.input_format = media::PixelFormat::NV12; // NV12 for GPU acceleration
    config.width = 1920;
    config.height = 1080;
    config.framerate = 30;
    config.bitrate = 4000000;  // 4 Mbps (HEVC is more efficient than H264)

    // Set HEVC-specific parameters
    media::codec::HEVCParams hevc_params;
    hevc_params.preset = "fast";
    hevc_params.profile = "main";
    hevc_params.level = "5.1";
    hevc_params.keyframe_interval = 120;
    hevc_params.crf = 28;
    hevc_params.constant_bitrate = false;
    hevc_params.max_b_frames = 4;
    hevc_params.threads = 0;
    config.SetHEVCParams(hevc_params);

    // Create the HEVC encoder instance using the factory method
    auto encoder = media::VideoEncoder::Create(config);
    if (!encoder) {
        std::cerr << "Failed to create GPU-accelerated HEVC encoder." << std::endl;
        return -1;
    }

    // Open the NV12 file
    std::ifstream file("/root/libmediacodec/examples/sample.yuv", std::ios::binary);
    if (!file) {
        std::cerr << "Error opening sample.yuv" << std::endl;
        return -1;
    }

    // Calculate the frame size for NV12 semi-planar format
    int width = config.width;
    int height = config.height;
    int y_size = width * height;
    int uv_size = width * height / 2; // UV is interleaved but half resolution
    int frame_size = y_size + uv_size;

    // Prepare a buffer for one frame
    std::vector<uint8_t> frame_data(frame_size);
    int frame_count = 0;

    // Create output file for the encoded HEVC stream
    std::ofstream output_file("output.hevc", std::ios::binary);
    if (!output_file) {
        std::cerr << "Error creating output file" << std::endl;
        return -1;
    }

    // Loop through the file, reading and encoding each frame
    while (file.read(reinterpret_cast<char*>(frame_data.data()), frame_size)) {
        std::vector<uint8_t> encoded_frame;
        if (!encoder->EncodeNV12(frame_data, &encoded_frame)) {
            std::cerr << "Failed to encode frame " << frame_count << std::endl;
            continue;
        }
        
        // Write encoded frame to file
        output_file.write(reinterpret_cast<const char*>(encoded_frame.data()), encoded_frame.size());
        
        std::cout << "Encoded frame " << frame_count 
                  << " size: " << encoded_frame.size() << " bytes" << std::endl;
        frame_count++;
    }

    // Flush any buffered frames
    std::vector<uint8_t> flushed_data;
    if (encoder->Flush(&flushed_data)) {
        // Write flushed data to file
        output_file.write(reinterpret_cast<const char*>(flushed_data.data()), flushed_data.size());
        std::cout << "Flushed data size: " << flushed_data.size() << " bytes" << std::endl;
    }

    file.close();
    output_file.close();
    
    std::cout << "Encoded " << frame_count << " frames to output.hevc" << std::endl;
    return 0;
}