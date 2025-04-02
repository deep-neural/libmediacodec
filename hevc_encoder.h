#ifndef MEDIA_HEVC_ENCODER_H_
#define MEDIA_HEVC_ENCODER_H_

#include <cstdint>
#include <memory>
#include <vector>
#include <cstring>
#include <string>

namespace media {

// Enum for HEVC encoder presets
enum class HEVCPreset {
    ULTRAFAST,
    SUPERFAST,
    VERYFAST,
    FASTER,
    FAST,
    MEDIUM,
    SLOW,
    SLOWER,
    VERYSLOW,
    PLACEBO
};

// Enum for HEVC profiles
enum class HEVCProfile {
    MAIN,
    MAIN_10,
    MAIN_STILL_PICTURE,
    REXT // Range extensions
};

// Enum for rate control modes
enum class RateControlMode {
    CRF,     // Constant Rate Factor
    CQP,     // Constant Quantization Parameter
    ABR,     // Average Bitrate
    CBR      // Constant Bitrate
};

// Enum for tier (main or high)
enum class HEVCTier {
    MAIN,
    HIGH
};

// Enum for tune options
enum class HEVCTune {
    NONE,
    PSNR,
    SSIM,
    GRAIN,
    ZEROLATENCY,
    FASTDECODE,
    ANIMATION
};

struct HEVCEncoderConfig {
    // Basic settings
    int width = 0;
    int height = 0;
    int bitrate = 0;  // in bits per second
    int framerate = 0;
    
    // Encoding preset (controls encoding speed vs compression efficiency)
    HEVCPreset preset = HEVCPreset::MEDIUM;
    
    // Profile and level settings
    HEVCProfile profile = HEVCProfile::MAIN;
    HEVCTier tier = HEVCTier::MAIN;
    float level = 0.0;  // 1.0, 2.0, 2.1, 3.0, 3.1, 4.0, 4.1, 5.0, 5.1, 5.2, 6.0, 6.1, 6.2
    
    // Rate control settings
    RateControlMode rc_mode = RateControlMode::ABR;
    int crf = 23;         // Constant rate factor (0-51, lower means better quality)
    int qp = 23;          // Quantization parameter (0-51, lower means better quality)
    int max_bitrate = 0;  // Maximum bitrate for VBR mode (in bits per second)
    int buffer_size = 0;  // Size of the VBV buffer (in bits)
    int vbv_maxrate = 0;  // Maximum rate the VBV buffer should be assumed to fill at
    int vbv_bufsize = 0;  // Size of the VBV buffer
    
    // GOP (Group of Pictures) structure
    int keyint_max = 250;   // Maximum GOP size (maximum interval between keyframes)
    int keyint_min = 25;    // Minimum GOP size
    int scenecut = 40;      // Scene cut threshold
    bool open_gop = false;  // Open GOP configuration
    int bframes = 4;        // Number of B-frames between I and P
    bool b_pyramid = true;  // Use B-frames as references
    
    // Quality and analysis settings
    HEVCTune tune = HEVCTune::NONE;  // Tune settings for particular content type
    bool aq_mode = true;             // Enable adaptive quantization
    int aq_strength = 1;             // Strength of AQ (0-3)
    bool psy = true;                 // Enable psychovisual optimization
    int psy_rd = 1.0;                // Strength of psychovisual optimization
    int psy_rdoq = 1.0;              // Strength of psychovisual optimization in RDO quantization
    
    // Motion estimation settings
    int me_range = 57;       // Motion estimation search range
    bool subme = true;       // Subpixel motion estimation and mode decision
    int subme_level = 3;     // Subpixel refinement level (0-7)
    int me_method = 1;       // Motion estimation method (0: dia, 1: hex, 2: umh, 3: star, 4: full)
    
    // Slice and parallelism settings
    int slice_max_size = 0;  // Maximum slice size in bytes (0 = unlimited)
    int slice_max_count = 0; // Maximum number of slices (0 = unlimited)
    int threads = 0;         // Number of threads (0 = auto)
    
    // Deblocking filter settings
    bool deblock = true;     // Enable deblocking filter
    int deblock_alpha = 0;   // Deblocking filter Alpha offset (-6 to 6)
    int deblock_beta = 0;    // Deblocking filter Beta offset (-6 to 6)
    
    // SAO settings
    bool sao = true;         // Enable Sample Adaptive Offset
    
    // Misc settings
    bool repeat_headers = false;  // Repeat headers (SPS, PPS) with each keyframe
    bool annexb = true;           // Use Annex-B output format (vs. MP4/MOV format)
    int log_level = -1;           // FFmpeg log level (-1 = default)
    
    // HEVC-specific settings
    bool strong_intra_smoothing = true;  // Strong intra smoothing for 32x32 blocks
    bool constrained_intra = false;      // Constrained intra prediction
    bool cu_lossless = false;            // Use lossless coding for some CUs
    bool early_skip = true;              // Early SKIP detection
    
    // VUI (Video Usability Information) settings
    bool vui_parameters = true;          // Include VUI parameters
    bool fullrange = false;              // Use full range (vs. limited range)
    
    // Frames to encode
    int frames = 0;                     // Number of frames to encode (0 = all)
};

class HEVCEncoder {
public:
    // Creates a new HEVC encoder with the given configuration.
    // Returns nullptr if the encoder cannot be created.
    static std::unique_ptr<HEVCEncoder> Create(const HEVCEncoderConfig& config);

    // Destructor
    virtual ~HEVCEncoder() = default;

    // Encodes a frame in YUV420 format.
    // Input: yuv_data contains the YUV420 data
    // Output: encoded_frame will be filled with the encoded HEVC data
    // Returns: 1 on success, 0 on failure
    virtual int EncodeYUV420(const std::vector<uint8_t>& yuv_data, 
                            std::vector<uint8_t>* encoded_frame) = 0;

    // Flush any buffered frames
    virtual int Flush(std::vector<uint8_t>* encoded_frame) = 0;
    
    // Get current encoding stats
    virtual void GetStats(int* frames_encoded, double* avg_bitrate) const = 0;
    
    // Update encoder parameters mid-stream (only some parameters can be changed)
    virtual bool UpdateParams(int new_bitrate, int new_framerate) = 0;
};

}  // namespace media

#endif  // MEDIA_HEVC_ENCODER_H_