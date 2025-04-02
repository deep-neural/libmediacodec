// av1_decoder.h
#ifndef MEDIA_AV1_DECODER_H_
#define MEDIA_AV1_DECODER_H_

#include <memory>
#include <vector>
#include <string>

namespace media {

struct AV1DecoderConfig {
  // Thread management
  int threads = 1;                    // Number of threads to use for decoding
  bool frame_parallel = false;        // Enable frame-parallel decoding
  
  // Performance settings
  bool low_delay = false;             // Low delay decoding mode
  int skip_frames = 0;                // Number of frames to skip (0=none, 1=nonref, 2=bidir, 3=nonkey, 4=all)
  int skip_loop_filter = 0;           // Skip loop filtering (0=none, 1=nonref, 2=bidir, 3=nonkey, 4=all)
  int skip_idct = 0;                  // Skip IDCT/dequantization (0=none, 1=nonref, 2=bidir, 3=nonkey, 4=all)
  
  // Visual quality settings
  bool enable_film_grain = true;      // Enable film grain application
  bool enable_annex_b = false;        // Enable Annex-B format parsing
  bool enable_cdef = true;            // Enable Constrained Directional Enhancement Filter
  bool enable_restoration = true;     // Enable loop restoration filters
  bool enable_warped_motion = true;   // Enable warped motion
  bool enable_ref_frame_mvs = true;   // Enable reference frame motion vectors
  bool enable_intrabc = true;         // Enable intra block copy
  bool enable_masked_comp = true;     // Enable masked compound predictions
  bool enable_dual_filter = true;     // Enable vertical and horizontal filters
  bool enable_order_hint = true;      // Enable order hint
  bool enable_jnt_comp = true;        // Enable joint compound prediction
  
  // Error resilience
  bool error_resilient = false;       // Enable error resilience features
  bool strict_std_compliance = false; // Strict standard compliance
  
  // Decoder implementation details
  int operating_point = 0;            // Operating point for scalable streams (0-31)
  int tile_threads = 1;               // Number of threads for tile decoding
  bool row_mt = false;                // Enable row-based multi-threading
  bool enable_uncompressed_header = true; // Enable uncompressed header
  
  // Post-processing
  bool enable_postproc = false;       // Enable post-processing
  int postproc_strength = 0;          // Post-processing strength (0-100)
  
  // Debugging
  bool debug_all = false;             // Enable all debugging info
  bool print_info = false;            // Print decoder info
  
  // Memory management
  int max_threads = 16;               // Maximum number of threads
  int max_frame_threads = 1;          // Maximum number of frame threads
  int max_tile_threads = 4;           // Maximum number of tile threads
  int max_memory = 0;                 // Maximum memory usage (0 = unlimited)
  
  // Custom buffer management
  bool use_external_buffers = false;  // Use external frame buffers
  int num_external_buffers = 0;       // Number of external buffers
  
  // Temporal layer control
  int max_temporal_layer = -1;        // Maximum temporal layer to decode (-1 = all)
  
  // Spatial layer control
  int max_spatial_layer = -1;         // Maximum spatial layer to decode (-1 = all)
  
  // Color conversion
  std::string color_primaries;        // Color primaries (e.g., "bt709", "bt2020")
  std::string color_trc;              // Transfer characteristics (e.g., "bt709", "pq")
  std::string colorspace;             // Colorspace (e.g., "bt709", "bt2020nc")
  std::string color_range;            // Color range (e.g., "tv", "pc")
};

class AV1Decoder {
 public:
  // Creates an AV1 decoder instance with the specified configuration
  static std::unique_ptr<AV1Decoder> Create(const AV1DecoderConfig& config);

  virtual ~AV1Decoder() = default;

  // Decodes the AV1 compressed frame into YUV420 format
  // Returns 1 on success, 0 on failure
  virtual int DecodeToYUV420(std::vector<uint8_t>& yuv_frame,
                             const std::vector<uint8_t>* av1_frame) = 0;
                            
  // Resets the decoder state
  virtual void Reset() = 0;
  
  // Returns the width of the decoded frame
  virtual int GetWidth() const = 0;
  
  // Returns the height of the decoded frame
  virtual int GetHeight() const = 0;
};

}  // namespace media

#endif  // MEDIA_AV1_DECODER_H_