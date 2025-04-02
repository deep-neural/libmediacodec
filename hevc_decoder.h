#ifndef MEDIA_HEVC_DECODER_H_
#define MEDIA_HEVC_DECODER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace media {

enum class DeinterlaceMode {
  NONE = 0,
  BLEND = 1,
  BOB = 2,
  ADAPTIVE = 3
};

struct HEVCDecoderConfig {
  // Threading options
  int threads = 0;  // 0 means auto-detect, otherwise specific thread count
  bool frame_threads = true;  // Use frame-level multi-threading

  // Latency and buffering options
  bool low_latency = false;
  int max_decode_queue_size = 5;  // Maximum number of frames to queue for decoding
  
  // Error handling options
  bool enable_error_concealment = true;
  bool skip_corrupted_frames = false;
  int error_resilience = 1;  // 0-none, 1-careful, 2-aggressive
  
  // Performance options
  bool fast_decode = false;  // Prioritize decoding speed over quality
  bool skip_loop_filter = false;  // Skip loop filtering for faster decoding
  int skip_frame = 0;  // 0-none, 1-default, 2-noref, 3-bidir, 4-nonintra, 5-all
  
  // Output format options
  bool output_10bit = false;  // Output in 10-bit format if available
  bool output_crop = true;  // Apply cropping information from bitstream
  bool preserve_alpha = false;  // Preserve alpha channel if present
  
  // Deinterlacing options
  DeinterlaceMode deinterlace_mode = DeinterlaceMode::NONE;
  
  // Debug options
  bool debug_mode = false;
  int debug_level = 0;  // 0-7, higher means more verbose
  
  // Post-processing options
  bool enable_post_processing = false;
  int post_processing_quality = 0;  // 0-highest quality, 6-fastest
  
  // Reference frame management
  int max_references = 16;  // Maximum reference frames to keep
  
  // Timing options
  bool respect_timing = true;  // Respect frame timing information
  
  // Slice options
  int max_slice_count = 0;  // Maximum number of slices to process (0 for unlimited)
  
  // Bitstream filter
  std::string bitstream_filters = "";  // Comma-separated list of bitstream filters
};

class HEVCDecoder {
 public:
  // Factory method to create the decoder
  static std::unique_ptr<HEVCDecoder> Create(const HEVCDecoderConfig& config);

  virtual ~HEVCDecoder() = default;

  // Decode a HEVC frame to YUV420 format
  // Returns 0 on error, positive value on success
  virtual int DecodeToYUV420(std::vector<uint8_t>* yuv_frame,
                            const std::vector<uint8_t>* hevc_frame) = 0;

  // Get frame width
  virtual int GetWidth() const = 0;

  // Get frame height
  virtual int GetHeight() const = 0;

  // Flush the decoder
  virtual void Flush() = 0;

  // Reset the decoder
  virtual void Reset() = 0;
  
  // Update runtime config (only for parameters that can be changed during decoding)
  virtual bool UpdateConfig(const HEVCDecoderConfig& config) = 0;
  
  // Get current configuration
  virtual HEVCDecoderConfig GetConfig() const = 0;
};

}  // namespace media

#endif  // MEDIA_HEVC_DECODER_H_