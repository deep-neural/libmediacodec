#ifndef H264_ENCODER_H_
#define H264_ENCODER_H_

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    }

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace media {

struct H264EncoderConfig {
    // Basic parameters
    int width = 1920;
    int height = 1080;
    int bitrate = 5000000;  // 5 Mbps
    int framerate = 30;
    
    // Preset and profile
    std::string preset = "medium";  // ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow
    std::string profile = "high";   // baseline, main, high, high10, high422, high444
    std::string level = "4.1";      // 1, 1b, 1.1, 1.2, 1.3, 2, 2.1, 2.2, 3, 3.1, 3.2, 4, 4.1, 4.2, 5, 5.1, 5.2, 6, 6.1, 6.2
    std::string tune = "";          // film, animation, grain, stillimage, fastdecode, zerolatency
    
    // GOP and frame structure
    int gop_size = 30;              // Group of pictures size
    int max_b_frames = 2;           // Maximum number of B-frames
    int refs = 3;                   // Number of reference frames
    bool open_gop = false;          // Open GOP
    int keyint_min = 25;            // Minimum GOP size
    int scenecut_threshold = 40;    // Scene cut detection threshold
    bool repeat_headers = false;    // Repeat SPS/PPS headers
    
    // Rate control
    bool constant_bitrate = false;  // Use CBR instead of VBR
    int crf = 23;                   // Constant Rate Factor (0-51, lower is better quality)
    int qp = -1;                    // Constant QP (0-51, -1 to disable)
    int rc_lookahead = 40;          // Rate control lookahead
    int vbv_maxrate = 0;            // Maximum bitrate for VBV
    int vbv_bufsize = 0;            // VBV buffer size
    float vbv_init = 0.9f;          // Initial VBV buffer occupancy
    float bitrate_variance = 0.0f;  // Variance in bitrate (0.0-1.0)
    float qp_variance = 0.0f;       // Variance in QP (0.0-1.0)
    int qp_min = 0;                 // Minimum QP
    int qp_max = 51;                // Maximum QP
    int qp_step = 4;                // Maximum QP step
    
    // Motion estimation
    std::string me_method = "hex";  // Motion estimation method: dia, hex, umh, esa, tesa
    int me_range = 16;              // Motion estimation range
    int subpixel_me = 7;            // Subpixel motion estimation quality
    int me_skip_threshold = 0;      // Motion estimation skip threshold
    
    // Analysis
    bool psy_rd = true;             // Use psychovisual rate distortion optimization
    float psy_rd_strength = 1.0f;   // Psychovisual RD strength (0.0-2.0)
    bool fast_pskip = true;         // Enable fast P-skip detection
    bool mixed_refs = true;         // Enable mixed reference frames
    bool cabac = true;              // Use CABAC entropy coding
    bool dct8x8 = true;             // Use 8x8 DCT transform
    bool aq_mode = true;            // Adaptive quantization mode
    float aq_strength = 1.0f;       // Adaptive quantization strength (0.0-3.0)
    bool deblock = true;            // Use deblocking filter
    int deblock_alpha = 0;          // Deblocking filter alpha (-6 to 6)
    int deblock_beta = 0;           // Deblocking filter beta (-6 to 6)
    
    // Slices and parallel processing
    int slices = 0;                 // Number of slices (0 for auto)
    int slice_max_size = 0;         // Maximum slice size in bytes (0 for unlimited)
    int threads = 0;                // Number of threads (0 for auto)
    
    // Metadata
    bool add_sei = true;            // Add SEI messages
    bool add_aud = false;           // Add access unit delimiter
    bool annexb = true;             // Output in Annex B format (vs. AVCC)
    
    // Intra refresh
    int intra_refresh = 0;          // Enable intra refresh instead of IDR frames (0 to disable)
    int keyint_sec = 0;             // Keyframe interval in seconds (0 to use gop_size)
    
    // Trellis quantization
    int trellis = 1;                // Trellis quantization (0=off, 1=final encode, 2=all)
    
    // Noise reduction
    int nr_strength = 0;            // Noise reduction strength (0 to disable)
    
    // Compatibility
    bool force_cfr = false;         // Force constant framerate
    bool bluray_compat = false;     // Enable Blu-ray compatibility
};

class H264Encoder {
public:
    // Factory method to create an encoder instance
    static std::unique_ptr<H264Encoder> Create(const H264EncoderConfig& config);
    
    virtual ~H264Encoder() = default;
    
    // Encodes a YUV420 frame and outputs compressed H.264 data
    // Returns true on success, false on failure
    virtual bool EncodeYUV420(const std::vector<uint8_t>& yuv_data, 
                             std::vector<uint8_t>* output_frame) = 0;
    
    // Flush any remaining frames (call when encoding is finished)
    virtual bool Flush(std::vector<uint8_t>* output_frame) = 0;
    
    // Reset the encoder with new configuration
    virtual bool Reconfigure(const H264EncoderConfig& config) = 0;
    
    // Get current configuration
    virtual H264EncoderConfig GetConfig() const = 0;
};

}  // namespace media

#endif  // H264_ENCODER_H_