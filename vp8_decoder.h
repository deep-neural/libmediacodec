// vp8_decoder.h
#ifndef VP8_DECODER_H
#define VP8_DECODER_H

#include <vector>
#include <cstdint>
#include <memory>
#include <string>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
}

struct VP8DecoderConfig {
    // Basic decoding parameters
    int width = 0;             // Width of the frame (0 for auto-detection)
    int height = 0;            // Height of the frame (0 for auto-detection)
    
    // Thread configuration
    int thread_count = 0;      // Number of threads to use (0 for auto)
    int thread_type = FF_THREAD_FRAME; // FF_THREAD_FRAME or FF_THREAD_SLICE
    
    // Error concealment and handling
    int error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK; // Error concealment flags
    int skip_loop_filter = AVDISCARD_DEFAULT; // Skip loop filter for speed
    int skip_idct = AVDISCARD_DEFAULT;       // Skip IDCT for speed
    int skip_frame = AVDISCARD_DEFAULT;      // Skip frame for speed
    
    // Low-level decoder settings
    int flags = 0;             // Decoder flags (AV_CODEC_FLAG_*)
    int flags2 = 0;            // Additional decoder flags (AV_CODEC_FLAG2_*)
    
    // Decoder output format
    int pixel_format = AV_PIX_FMT_YUV420P; // Output pixel format
    
    // Decoder mode
    bool low_delay = false;    // Low delay mode
    
    // Debugging
    int debug = 0;             // Debug flags
    
    // Algorithm-specific options
    int lowres = 0;            // Low resolution scaling (0 = off)
    
    // Frame rate
    AVRational framerate = {0, 1}; // Frame rate for decoding
    
    // Timebase
    AVRational timebase = {1, 1000000}; // Default timebase
    
    // Error resilience
    int err_recognition = AV_EF_CAREFUL; // Error recognition
    
    // Used for tuning decoder behavior
    std::string tune; // Tune settings for specific scenarios
    
    // Output parameters
    bool output_alpha = false; // Output alpha channel if present
    
    // Extradata (for codec initialization)
    std::vector<uint8_t> extradata;
};

class VP8Decoder {
public:
    static std::shared_ptr<VP8Decoder> Create(const VP8DecoderConfig& config);
    int DecodeToYUV420(const std::vector<uint8_t>& vp8_frame, std::vector<uint8_t>* yuv_data);

    // Make the destructor public
    ~VP8Decoder();

private:
    VP8Decoder();
    bool Initialize(const VP8DecoderConfig& config);

    AVCodecContext* codec_context_;
    AVFrame* frame_;
    AVPacket* packet_;
    VP8DecoderConfig config_;
};

#endif // VP8_DECODER_H