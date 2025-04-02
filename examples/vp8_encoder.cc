#include "media_video_encoder.h"
#include <iostream>
#include <fstream>
#include <vector>

int main() {
    // Configure encoder for VP8
    media::VideoEncoderConfig config;
    config.output_codec = media::CodecType::VP8;
    config.width = 1920;
    config.height = 1080;
    config.framerate = 30;
    config.bitrate = 5000000;

    // Set VP8-specific parameters
    media::codec::VP8Params vp8_params;
    vp8_params.quality = 10;             // Quality setting (0-best, 63-worst)
    vp8_params.keyframe_interval = 120;  // Maximum distance between keyframes
    config.SetVP8Params(vp8_params);

    // Create the VP8 encoder instance using the factory method
    auto encoder = media::VideoEncoder::Create(config);
    if (!encoder) {
        std::cerr << "Failed to create VP8 encoder." << std::endl;
        return -1;
    }

    // Open the YUV420 file (assumes "input.yuv" is in the working directory)
    std::ifstream file("/root/libmediacodec/examples/sample.yuv", std::ios::binary);
    if (!file) {
        std::cerr << "Error opening input.yuv" << std::endl;
        return -1;
    }

    // Calculate the frame size for YUV420 planar format
    int width = config.width;
    int height = config.height;
    int y_size = width * height;
    int uv_size = (width / 2) * (height / 2);
    int frame_size = y_size + 2 * uv_size;

    // Prepare a buffer for one frame
    std::vector<uint8_t> frame_data(frame_size);
    int frame_count = 0;

    // Loop through the file, reading and encoding each frame
    while (file.read(reinterpret_cast<char*>(frame_data.data()), frame_size)) {
        std::vector<uint8_t> encoded_frame;
        if (!encoder->EncodeYUV420(frame_data, &encoded_frame)) {
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
