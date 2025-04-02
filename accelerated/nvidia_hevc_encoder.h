#ifndef NVIDIA_HEVC_ENCODER_H_
#define NVIDIA_HEVC_ENCODER_H_

#include <memory>
#include <vector>
#include <cstdint>
#include <cstring>

// Forward declarations for FFmpeg structures
extern "C" {
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
}

namespace media {

/**
 * Configuration parameters for the NVIDIA HEVC encoder.
 */
struct NvidiaHEVCEncoderConfig {
    int width;                      // Video frame width in pixels
    int height;                     // Video frame height in pixels
    int bitrate;                    // Target bitrate in bits per second
    int framerate;                  // Video frame rate
    int gop_length = 30;            // Group of pictures length (I-frame interval)
    bool low_latency = false;       // Enable low latency encoding mode
    int quality_preset = 4;         // Quality preset (1-7, higher is better quality)
    int max_b_frames = 0;           // Maximum number of B frames (0 means no B frames)
    bool use_cbr = true;            // Use constant bitrate mode (false for VBR)
};

/**
 * A wrapper class for the NVIDIA HEVC hardware encoder using FFmpeg avcodec
 */
class NvidiaHEVCEncoder {
public:
    /**
     * Create an instance of the NVIDIA HEVC encoder.
     * 
     * @param config Configuration parameters for the encoder
     * @return A unique pointer to the encoder instance or nullptr if creation failed
     */
    static std::unique_ptr<NvidiaHEVCEncoder> Create(const NvidiaHEVCEncoderConfig& config);

    /**
     * Destructor.
     */
    ~NvidiaHEVCEncoder();

    /**
     * Encode a YUV420 format frame.
     *
     * @param yuv_data Input frame data in YUV420 format
     * @param encoded_frame Output encoded HEVC frame
     * @return True if encoding was successful, false otherwise
     */
    bool EncodeYUV420(const std::vector<uint8_t>& yuv_data, std::vector<uint8_t>* encoded_frame);

    /**
     * Encode a NV12 format frame.
     *
     * @param nv12_data Input frame data in NV12 format
     * @param encoded_frame Output encoded HEVC frame
     * @return True if encoding was successful, false otherwise
     */
    bool EncodeNV12(const std::vector<uint8_t>& nv12_data, std::vector<uint8_t>* encoded_frame);

private:
    // Private constructor - use Create() to instantiate
    explicit NvidiaHEVCEncoder(const NvidiaHEVCEncoderConfig& config);

    // Initialize the encoder
    bool Initialize(const NvidiaHEVCEncoderConfig& config);
    
    // Common encoding function used by both EncodeYUV420 and EncodeNV12
    bool EncodeFrame(const std::vector<uint8_t>& frame_data, 
                     std::vector<uint8_t>* encoded_frame, 
                     bool is_nv12);

    // FFmpeg structures
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

#endif  // NVIDIA_HEVC_ENCODER_H_