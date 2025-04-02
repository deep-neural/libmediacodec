#ifndef NVIDIA_AV1_ENCODER_H_
#define NVIDIA_AV1_ENCODER_H_

#include <memory>
#include <vector>
#include <cstdint>
#include <cstring>

// Forward declarations for FFmpeg structs
extern "C" {
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
}

namespace media {

/**
 * Configuration parameters for the NVIDIA AV1 encoder.
 */
struct NvidiaAV1EncoderConfig {
    int width;                      // Video frame width in pixels
    int height;                     // Video frame height in pixels
    int bitrate;                    // Target bitrate in bits per second
    int framerate;                  // Video frame rate
    int gop_length = 30;            // Group of pictures length (key-frame interval)
    bool low_latency = false;       // Enable low latency encoding mode
    int quality_preset = 4;         // Quality preset (1-7, higher is better quality)
    bool use_cbr = true;            // Use constant bitrate mode (false for VBR)
    int max_num_ref_frames = 4;     // Maximum number of reference frames
    bool enable_film_grain = false; // Enable film grain synthesis
    int tile_columns = 0;           // Number of tile columns (0 for automatic selection)
    int tile_rows = 0;              // Number of tile rows (0 for automatic selection)
};

/**
 * A wrapper class for the NVIDIA AV1 hardware encoder using FFmpeg/avcodec
 */
class NvidiaAV1Encoder {
public:
    /**
     * Create an instance of the NVIDIA AV1 encoder.
     * 
     * @param config Configuration parameters for the encoder
     * @return A unique pointer to the encoder instance or nullptr if creation failed
     */
    static std::unique_ptr<NvidiaAV1Encoder> Create(const NvidiaAV1EncoderConfig& config);

    /**
     * Destructor.
     */
    ~NvidiaAV1Encoder();

    /**
     * Encode a YUV420 format frame.
     *
     * @param yuv_data Input frame data in YUV420 format
     * @param encoded_frame Output encoded AV1 frame
     * @return True if encoding was successful, false otherwise
     */
    bool EncodeYUV420(const std::vector<uint8_t>& yuv_data, std::vector<uint8_t>* encoded_frame);

    /**
     * Encode a NV12 format frame.
     *
     * @param nv12_data Input frame data in NV12 format
     * @param encoded_frame Output encoded AV1 frame
     * @return True if encoding was successful, false otherwise
     */
    bool EncodeNV12(const std::vector<uint8_t>& nv12_data, std::vector<uint8_t>* encoded_frame);

private:
    // Private constructor - use Create() to instantiate
    explicit NvidiaAV1Encoder(const NvidiaAV1EncoderConfig& config);

    // Initialize the encoder
    bool Initialize(const NvidiaAV1EncoderConfig& config);
    
    // Common encoding function used by both EncodeYUV420 and EncodeNV12
    bool EncodeFrame(const std::vector<uint8_t>& frame_data, 
                     std::vector<uint8_t>* encoded_frame, 
                     bool is_nv12);

    // FFmpeg objects
    AVCodecContext* codec_context_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVPacket* packet_ = nullptr;
    
    // Encoder parameters
    int width_ = 0;
    int height_ = 0;
    int frame_size_ = 0;
    int y_plane_size_ = 0;
    int64_t pts_ = 0;
    bool initialized_ = false;
};

}  // namespace media

#endif  // NVIDIA_AV1_ENCODER_H_