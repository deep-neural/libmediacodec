#ifndef VP8_ENCODER_H
#define VP8_ENCODER_H

#include <vector>
#include <stdint.h>
#include <string>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
}

namespace media {

// Enhanced VP8 encoder configuration with all possible parameters
struct VP8EncoderConfig {
    // Basic parameters
    int width;                  // Width of the frame
    int height;                 // Height of the frame
    int bitrate;                // Target bitrate in bits/second
    int framerate;              // Frame rate

    // Quality and bitrate control
    int quality;                // Quality setting (0-best, 63-worst)
    int min_quantizer;          // Minimum quantizer (0-63)
    int max_quantizer;          // Maximum quantizer (0-63)
    int buffer_size;            // Rate control buffer size
    float buffer_initial_size;  // Initial buffer fullness (0.0-1.0)
    float buffer_optimal_size;  // Optimal buffer fullness (0.0-1.0)
    
    // Keyframe settings
    int keyframe_interval;      // Maximum distance between keyframes in frames
    int keyframe_min_interval;  // Minimum distance between keyframes in frames
    bool auto_keyframe;         // Enable automatic keyframe placement
    
    // Multi-threading settings
    int thread_count;           // Number of threads to use for encoding
    
    // Rate control settings
    enum RateControlMode {
        RC_MODE_VBR,           // Variable Bit Rate
        RC_MODE_CBR,           // Constant Bit Rate
        RC_MODE_CQ,            // Constant Quality
    };
    RateControlMode rc_mode;    // Rate control mode
    
    // Error resilience
    bool error_resilient;       // Enable error resilient mode
    
    // Deadline/speed control
    enum Deadline {
        DEADLINE_BEST_QUALITY = 0,
        DEADLINE_GOOD_QUALITY = 1,
        DEADLINE_REALTIME = 2
    };
    Deadline deadline;          // Encoding speed/quality tradeoff
    
    // CPU usage (0-16, higher = faster encoding, lower quality)
    int cpu_used;
    
    // Noise sensitivity (0-100, higher = more denoising)
    int noise_sensitivity;
    
    // Sharpness (0-7, higher = sharper)
    int sharpness;
    
    // Static threshold for encoding macroblocks
    int static_threshold;
    
    // Token partitions (0-3, 2^value partitions)
    int token_partitions;
    
    // Arnr (Adaptive non-real-time encoding) settings
    bool arnr_enabled;         // Enable temporal filtering for noise reduction
    int arnr_max_frames;       // Maximum number of frames to filter
    int arnr_strength;         // Filtering strength (0-6)
    int arnr_type;             // Filter type
    
    // Lag in frames (0 = disabled)
    int lag_in_frames;
    
    // 2-pass encoding
    bool two_pass_encoding;    // Enable two-pass encoding
    std::string stats_file;    // First pass stats file path

    // Default constructor with reasonable defaults
    VP8EncoderConfig() 
        : width(640), 
          height(480), 
          bitrate(1000000), 
          framerate(30),
          quality(10),
          min_quantizer(4),
          max_quantizer(63),
          buffer_size(0),
          buffer_initial_size(0.9f),
          buffer_optimal_size(0.75f),
          keyframe_interval(300),
          keyframe_min_interval(0),
          auto_keyframe(true),
          thread_count(0),
          rc_mode(RC_MODE_VBR),
          error_resilient(false),
          deadline(DEADLINE_GOOD_QUALITY),
          cpu_used(0),
          noise_sensitivity(0),
          sharpness(0),
          static_threshold(0),
          token_partitions(0),
          arnr_enabled(false),
          arnr_max_frames(0),
          arnr_strength(3),
          arnr_type(1),
          lag_in_frames(0),
          two_pass_encoding(false),
          stats_file("") {}
};

class VP8Encoder {
public:
    static VP8Encoder* Create(const VP8EncoderConfig& config);
    ~VP8Encoder();

    int EncodeYUV420(const std::vector<uint8_t>& yuv_data, std::vector<uint8_t>* encoded_frame);
    
    // For two-pass encoding
    bool StartFirstPass();
    bool StartSecondPass();
    bool IsFirstPassComplete() const;

private:
    VP8Encoder(const VP8EncoderConfig& config);
    
    // Apply config settings to codec context
    bool ApplyCodecOptions(const VP8EncoderConfig& config);
    
    bool initialized_;
    bool first_pass_complete_;
    VP8EncoderConfig config_;
    AVCodecContext* codec_context_;
};

} // namespace media

#endif // VP8_ENCODER_H