#ifndef VP9_ENCODER_H_
#define VP9_ENCODER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace media {

// Enum for VP9 quality modes
enum class VP9Quality {
  REALTIME,  // Realtime encoding
  GOOD,      // Good quality
  BEST       // Best quality
};

// Enum for VP9 profile
enum class VP9Profile {
  PROFILE_0,  // 8-bit 4:2:0
  PROFILE_1,  // 8-bit 4:2:2 or 4:4:4
  PROFILE_2,  // 10/12-bit 4:2:0
  PROFILE_3   // 10/12-bit 4:2:2 or 4:4:4
};

struct VP9EncoderConfig {
  // Basic settings
  int width = 0;
  int height = 0;
  int bitrate = 0;        // Target bitrate in bits per second
  int framerate = 30;     // Frame rate in frames per second
  
  // Rate control settings
  bool use_cbr = false;             // Use constant bitrate mode if true, VBR if false
  int max_bitrate = 0;              // Maximum bitrate for VBR mode
  int min_bitrate = 0;              // Minimum bitrate for VBR mode
  int buffer_size = 0;              // Size of the rate control buffer (determines max bitrate fluctuation)
  int buffer_initial_size = 0;      // Initial buffer fullness
  
  // Quality settings
  VP9Quality quality = VP9Quality::GOOD;  // Quality preset
  int crf = 23;                           // Constant Rate Factor (0-63), 0 is lossless, 63 is worst
  int speed = 0;                          // Encoding speed (0-8), 0 is slowest/best, 8 is fastest
  bool lossless = false;                  // Lossless encoding mode
  
  // GOP (Group of Pictures) structure
  int keyframe_interval = 120;            // Maximum distance between keyframes
  bool auto_alt_ref = true;               // Enable/disable automatic alternate reference frames
  int lag_in_frames = 25;                 // Number of frames to look ahead for alternate reference frame selection
  
  // Threading settings
  int tile_columns = 0;                   // Log2 of number of tile columns (0-6)
  int tile_rows = 0;                      // Log2 of number of tile rows (0-2)
  bool frame_parallel = false;            // Enable frame parallel decodability features
  int threads = 0;                        // Number of threads to use (0 = auto)
  
  // Visual quality tuning
  bool error_resilient = false;           // Enable error resilient mode
  bool arnr_enabled = false;              // Enable altref noise reduction filter
  int arnr_strength = 3;                  // AltRef filter strength (0-6)
  int arnr_max_frames = 7;                // Max number of frames for AltRef noise reduction
  
  // Profile settings
  VP9Profile profile = VP9Profile::PROFILE_0;  // VP9 profile
  int bit_depth = 8;                          // Bit depth (8, 10, 12)
  
  // ROI (Region of Interest) settings
  bool roi_enabled = false;                   // Enable ROI-based encoding
  
  // SVC (Scalable Video Coding) settings
  bool svc_enabled = false;                   // Enable SVC encoding
  int svc_layers = 1;                         // Number of spatial layers
  int svc_temporal_layers = 1;                // Number of temporal layers
  
  // Advanced tuning parameters
  bool aq_mode = true;                        // Enable adaptive quantization
  int undershoot_pct = 100;                   // Datarate undershoot (min) target (%)
  int overshoot_pct = 100;                    // Datarate overshoot (max) target (%)
  int max_intra_bitrate_pct = 0;              // Max I-frame bitrate (pct) 0=unlimited
  int max_inter_bitrate_pct = 0;              // Max P-frame bitrate (pct) 0=unlimited
  bool row_mt = true;                         // Enable row-based multi-threading
};

class VP9Encoder {
 public:
  // Creates a VP9Encoder with the specified configuration.
  // Returns nullptr if encoder creation fails.
  static std::unique_ptr<VP9Encoder> Create(const VP9EncoderConfig& config);

  virtual ~VP9Encoder() = default;

  // Encodes a raw YUV420 frame.
  // |yuv_data| must be a buffer containing a full YUV420 frame with size:
  // (width * height * 3) / 2 bytes.
  // On success, |encoded_frame| will contain the compressed VP9 frame data.
  // Returns true on success, false on failure.
  virtual bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                           std::vector<uint8_t>* encoded_frame) = 0;

  // Returns the current configuration of the encoder.
  virtual const VP9EncoderConfig& GetConfig() const = 0;
  
  // Set a new target bitrate at runtime
  virtual bool UpdateBitrate(int new_bitrate) = 0;
  
  // Set a new framerate at runtime
  virtual bool UpdateFramerate(int new_framerate) = 0;
};

}  // namespace media

#endif  // VP9_ENCODER_H_