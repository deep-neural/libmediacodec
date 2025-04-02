#ifndef VP9_DECODER_H_
#define VP9_DECODER_H_

#include <memory>
#include <vector>
#include <cstdint>
#include <string>

namespace media {

struct VP9DecoderConfig {
  // Decoder threading configuration
  int threads = 1;  // Number of threads to use for decoding
  bool frame_threading = true;  // Use frame-based threading (default)
  bool slice_threading = false;  // Use slice-based threading
  
  // Performance/latency options
  bool low_delay = false;  // Low delay mode
  int skip_loop_filter = 0;  // Skip loop filtering; 0=none, 1=noref, 2=bidir, 3=nonkey, 4=all
  int skip_frame = 0;  // Skip frames: 0=none, 1=nonref, 2=bidir, 3=nonkey, 4=all
  
  // Error resilience/concealment
  bool error_concealment = false;  // Apply error concealment/recovery techniques
  int max_error_rate = 0;  // Error detection threshold (0-100)
  bool discard_corrupted_frames = false;  // Discard frames that contain corruption
  
  // Memory management
  int max_threads_per_tile = 0;  // Maximum number of threads per tile (0=auto)
  int max_tile_cols = 0;  // Maximum tile columns (0=auto)
  int max_tile_rows = 0;  // Maximum tile rows (0=auto)
  
  // Decoding parameters
  int max_width = 0;  // Maximum width (0=unlimited)
  int max_height = 0;  // Maximum height (0=unlimited)
  bool enable_film_grain = true;  // Enable film grain application (if present in stream)
  
  // Post-processing
  bool enable_post_processing = true;  // Enable decoder post-processing
  int deblocking_strength = 0;  // Deblocking filter strength (0-16, 0=off)
  bool deringing = false;  // Enable deringing filter
  
  // Debug options
  bool debug_visualization = false;  // Enable debug visualization
  int debug_level = 0;  // Debug information level (0-9)
  std::string dump_frames_path = "";  // Path to dump decoded frames (empty=off)
  
  // Color conversion options
  bool full_range = false;  // Output YUV in full range (0-255) instead of limited (16-235)
  int color_primaries = 0;  // Color primaries override (0=from stream)
  int color_trc = 0;  // Transfer characteristics override (0=from stream)
  int colorspace = 0;  // Colorspace override (0=from stream)
  
  // Reference frame management
  int max_references = 8;  // Maximum reference frames (1-8)
  
  // Extension for future additions without breaking ABI
  void* reserved = nullptr;
};

class VP9Decoder {
 public:
  // Factory method to create a decoder instance
  static std::unique_ptr<VP9Decoder> Create(const VP9DecoderConfig& config);

  virtual ~VP9Decoder() = default;

  // Decode a VP9 frame to YUV420 format
  // Returns 1 on success, 0 on failure
  virtual int DecodeToYUV420(const std::vector<uint8_t>& vp9_frame,
                            std::vector<uint8_t>* yuv_data) = 0;

  // Get the width of the decoded frames
  virtual int GetWidth() const = 0;

  // Get the height of the decoded frames
  virtual int GetHeight() const = 0;

  // Reset the decoder state
  virtual void Reset() = 0;
  
  // Update configuration parameters
  virtual bool UpdateConfig(const VP9DecoderConfig& config) = 0;
  
  // Get current configuration
  virtual VP9DecoderConfig GetConfig() const = 0;
};

}  // namespace media

#endif  // VP9_DECODER_H_