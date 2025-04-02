#ifndef MEDIA_AV1_ENCODER_H_
#define MEDIA_AV1_ENCODER_H_

#include <cstdint>
#include <memory>
#include <vector>
#include <cstring>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

namespace media {

// Enumeration for AV1 available presets
enum class AV1SpeedPreset {
  SLOWEST = 0,
  SLOWER = 1,
  SLOW = 2,
  MEDIUM = 4,
  FAST = 6,
  FASTER = 8,
  FASTEST = 10
};

// Enumeration for AV1 tune options
enum class AV1TuneMode {
  NONE,
  PSNR,
  SSIM,
  VMAF,
  FILM_GRAIN
};

// Enumeration for rate control modes
enum class AV1RateControlMode {
  CRF,      // Constant Rate Factor
  CBR,      // Constant Bitrate
  VBR,      // Variable Bitrate
  CQP       // Constant Quantizer
};

// Enumeration for tile configurations
enum class AV1TileConfig {
  AUTO,     // Automatically determine based on threads and resolution
  SINGLE,   // Single tile
  MAXIMUM   // Maximum possible tiles
};

struct AV1EncoderConfig {
  // Required parameters
  int width;
  int height;
  int bitrate;
  int framerate;

  // Basic encoding parameters
  int keyframe_interval = 120;  // Default: every 2s at 60fps
  int threads = 4;              // Default: 4 encoding threads
  int crf = 23;                 // Constant Rate Factor (quality, lower is better)
  
  // AV1 specific parameters
  AV1SpeedPreset speed_preset = AV1SpeedPreset::MEDIUM;  // Speed vs quality tradeoff
  AV1TuneMode tune_mode = AV1TuneMode::NONE;             // Tuning mode
  AV1RateControlMode rc_mode = AV1RateControlMode::CRF;  // Rate control mode
  AV1TileConfig tile_config = AV1TileConfig::AUTO;       // Tile configuration
  
  // Rate control parameters
  int qp = 30;                  // Quantization parameter (for CQP mode)
  int min_q = 0;                // Minimum quantizer
  int max_q = 63;               // Maximum quantizer
  int vbr_target_percentage = 70; // Target percentage of max bitrate for VBR
  int bitrate_undershoot = 95;  // Bitrate undershoot percentage
  int bitrate_overshoot = 105;  // Bitrate overshoot percentage
  
  // Tile and threading parameters
  int tile_columns = 0;         // Number of tile columns (0=auto)
  int tile_rows = 0;            // Number of tile rows (0=auto)
  int row_mt = 1;               // Enable row-based multi-threading (0/1)
  
  // GOP (Group of Pictures) structure
  int max_intra_rate = 0;       // Maximum I-frame rate (0=auto)
  bool use_fixed_qp_offsets = false; // Use fixed QP offsets for frame types
  int keyframe_qp_offset = 0;   // QP offset for keyframes
  int max_reference_frames = 3; // Maximum number of reference frames
  
  // Visual quality parameters
  int arnr_strength = 3;        // Altref noise reduction filter strength
  int arnr_maxframes = 7;       // Maximum number of frames for altref noise reduction
  bool enable_cdef = true;      // Enable Constrained Directional Enhancement Filter
  bool enable_restoration = true; // Enable loop restoration filter
  bool enable_film_grain = false; // Enable film grain synthesis
  int film_grain_strength = 0;  // Film grain strength (0-50)
  bool enable_tpl = true;       // Enable temporal pattern coding
  
  // Color parameters
  int color_range = 0;          // Color range (0=limited, 1=full)
  
  // Complexity parameters
  bool enable_superblock_split = true; // Allow more aggressive superblock splits
  bool enable_rect_partitions = true;  // Allow rectangular partitions
  bool enable_1to4_partitions = true;  // Allow 1:4 and 4:1 partitions
  bool enable_cfl = true;              // Chroma from luma prediction

  // Custom error resilience features
  bool error_resilient_mode = false;  // Error resilient mode
  bool frame_parallel_decoding = false; // Enable frame parallel decoding
  
  // Perceptual tuning
  bool tune_content = false;    // Tune for specific content type
  std::string content_type = "default"; // Content type (default/screen/film/animation)
};

class AV1Encoder {
 public:
  // Factory method to create an encoder instance
  static std::unique_ptr<AV1Encoder> Create(const AV1EncoderConfig& config);

  // Destructor
  ~AV1Encoder();

  // Encodes YUV420 format data and writes encoded frame to output
  // Returns true on success, false on failure
  bool EncodeYUV420(const std::vector<uint8_t>& yuv_data, 
                   std::vector<uint8_t>* output_frame);

  // Flush the encoder to get any pending frames
  bool Flush(std::vector<uint8_t>* output_frame);

 private:
  // Private constructor, use Create() instead
  AV1Encoder();

  // Initializes the encoder with the provided configuration
  bool Initialize(const AV1EncoderConfig& config);

  // Process encoded packets
  bool ProcessEncodedPacket(AVPacket* packet, std::vector<uint8_t>* output_frame);

  // Helper to set all config parameters
  bool SetEncoderParameters();

  // Internal configuration and state
  AV1EncoderConfig config_;
  AVCodecContext* codec_context_ = nullptr;
  AVFrame* frame_ = nullptr;
  int64_t pts_ = 0;
  bool initialized_ = false;
};

}  // namespace media

#endif  // MEDIA_AV1_ENCODER_H_