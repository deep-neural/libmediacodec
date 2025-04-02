#include "nvidia_av1_encoder.h"
#include <iostream>
#include <fstream>
#include <vector>

int main() {
    // Configure encoder for NVIDIA AV1
    media::NvidiaAV1EncoderConfig config;
    config.width = 1920;
    config.height = 1080;
    config.framerate = 30;
    config.bitrate = 5000000;
    
    // Set AV1-specific parameters
    config.gop_length = 120;        // Key-frame interval (similar to H264's keyframe_interval)
    config.quality_preset = 5;      // Medium-high quality (range is 1-7)
    config.use_cbr = false;         // Using VBR mode instead of CBR (like H264 example)
    config.max_num_ref_frames = 4;
    config.enable_film_grain = false;
    config.tile_columns = 2;        // Splitting the frame into 2 columns for parallelism 
    config.tile_rows = 1;           // Using 1 row
    config.low_latency = false;     // Not using low latency mode for better compression

    // Create the NVIDIA AV1 encoder instance using the factory method
    auto encoder = media::NvidiaAV1Encoder::Create(config);
    if (!encoder) {
        std::cerr << "Failed to create NVIDIA AV1 encoder." << std::endl;
        return -1;
    }

    // Open the YUV420 file (assumes "sample.yuv" is in the working directory)
    std::ifstream file("/root/libmediacodec/examples/sample.yuv", std::ios::binary);
    if (!file) {
        std::cerr << "Error opening sample.yuv" << std::endl;
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

    // Note: The NvidiaAV1Encoder doesn't have a Flush method in the header
    // If you need to flush buffered frames, you'd need to add this method
    // Similar to:
    // std::vector<uint8_t> flushed_data;
    // if (encoder->Flush(&flushed_data)) {
    //     std::cout << "Flushed data size: " << flushed_data.size() << " bytes" << std::endl;
    // }

    file.close();
    return 0;
}