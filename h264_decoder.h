#ifndef H264_DECODER_H_
#define H264_DECODER_H_

#include <cstdint>
#include <memory>
#include <vector>
#include <mutex>

namespace media {

// Configuration options for the H264 decoder
struct H264DecoderConfig {
  // Frame dimensions (can be 0 if unknown, will be detected from stream)
  int width = 0;
  int height = 0;
  
  // Thread count for decoding (0 = auto)
  int thread_count = 0;
  
  // Low delay mode (reduces latency but may affect compression efficiency)
  bool low_delay = false;

  // Skip loop filter for performance reasons
  bool skip_loop_filter = false;
  
  // Skip frame (bidir predicted frames)
  bool skip_frame = false;
  
  // Error concealment flags
  bool error_concealment = false;
  
  // Skip idct
  bool skip_idct = false;
  
  // Error recognition flags
  int error_recognition = 0;
  
  // Number of frames to skip after a decoder flush
  int skip_frames_after_flush = 0;
  
  // Maximum number of reference frames
  int max_refs = 0;
  
  // Enable slice-based multithreading
  bool slice_threads = false;
  
  // Enable frame-based multithreading
  bool frame_threads = true;
  
  // Range of QP values to use (0-51)
  int qp_min = 0;
  int qp_max = 0;
  
  // Maximum B-frames between I and P
  int max_b_frames = 0;
  
  // Custom extradata buffer (SPS/PPS)
  std::vector<uint8_t> extradata;
  
  // Request a specific pixel format (FFmpeg format constants)
  int pixel_format = -1;
  
  // Decoder delay (in frames)
  int delay = 0;
  
  // Reference frame count (affects memory usage)
  int refs = 0;
  
  // Stream-level profile and level idc values
  int profile = -1;
  int level = -1;
  
  // Output decoded pictures in display order (not decoding order)
  bool output_in_display_order = true;
  
  // Enable strict standard compliance
  bool strict_std_compliance = false;
  
  // Log level for the decoder (negative means no output)
  int log_level = -8;
};

// H264 to YUV420 decoder class that uses FFmpeg's libavcodec
class H264Decoder {
 public:
  // Factory method to create a decoder instance
  static std::unique_ptr<H264Decoder> Create(const H264DecoderConfig& config);
  
  // Non-copyable
  H264Decoder(const H264Decoder&) = delete;
  H264Decoder& operator=(const H264Decoder&) = delete;
  
  // Virtual destructor to allow proper cleanup in derived classes
  virtual ~H264Decoder() = default;
  
  // Decode a H264 frame to YUV420 format
  // Returns 1 on success, 0 if more data is needed, negative value on error
  virtual int DecodeToYUV420(std::vector<uint8_t>& yuv_frame, const std::vector<uint8_t>* h264_frame) = 0;
  
  // Reset the decoder state
  virtual void Reset() = 0;
  
  // Get the dimensions of the last decoded frame
  virtual void GetFrameDimensions(int* width, int* height) const = 0;
  
  // Check if the decoder is properly initialized
  virtual bool IsInitialized() const = 0;

 protected:
  // Protected constructor for implementation classes
  H264Decoder() = default;
};

}  // namespace media

#endif  // H264_DECODER_H_