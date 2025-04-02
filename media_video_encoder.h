#ifndef MEDIA_VIDEO_ENCODER_H_
#define MEDIA_VIDEO_ENCODER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace media {

// Supported pixel formats for input
enum class PixelFormat {
  YUV420,  // Planar YUV 4:2:0
  NV12     // Semi-planar YUV 4:2:0 (Y + interleaved UV)
};

// Supported video codecs
enum class CodecType {
  H264,    // H.264 / AVC
  HEVC,    // H.265 / HEVC
  VP8,     // VP8
  VP9,     // VP9
  AV1      // AV1
};

// Base struct for codec-specific params
namespace codec {

struct BaseCodecParams {
  virtual ~BaseCodecParams() = default;
};

struct H264Params : public BaseCodecParams {
  std::string preset = "medium";  // ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow
  std::string profile = "high";   // baseline, main, high
  std::string level = "4.1";      // 1, 1b, 1.1, 1.2, 1.3, 2, 2.1, 2.2, 3, 3.1, 3.2, 4, 4.1, 4.2, 5, 5.1, 5.2, 6, 6.1, 6.2
  int keyframe_interval = 120;    // Keyframe interval
  int max_b_frames = 2;           // Maximum number of B-frames
  bool constant_bitrate = false;  // Use CBR instead of VBR
  int crf = 23;                   // Constant Rate Factor (quality, lower is better quality)
  int threads = 0;                // Number of threads (0 = auto)
};

struct HEVCParams : public BaseCodecParams {
  std::string preset = "medium";  // ultrafast, superfast, veryfast, faster, fast, medium, slow, slower, veryslow, placebo
  std::string profile = "main";   // main, main10, main-still-picture, rext
  std::string level = "4.1";      // 1.0, 2.0, 2.1, 3.0, 3.1, 4.0, 4.1, 5.0, 5.1, 5.2, 6.0, 6.1, 6.2
  int keyframe_interval = 120;    // Keyframe interval
  int crf = 28;                   // Constant Rate Factor (quality, lower is better quality)
  bool constant_bitrate = false;  // Use CBR instead of VBR
  int max_b_frames = 4;           // Number of B-frames between I and P
  int threads = 0;                // Number of threads (0 = auto)
};

struct VP8Params : public BaseCodecParams {
  int quality = 10;               // Quality setting (0-best, 63-worst)
  int keyframe_interval = 120;    // Maximum distance between keyframes in frames
  bool constant_bitrate = false;  // Use CBR instead of VBR
  int threads = 0;                // Number of threads (0 = auto)
};

struct VP9Params : public BaseCodecParams {
  int quality = 23;               // Quality setting (0-63, lower is better)
  std::string speed = "good";     // realtime, good, best
  std::string profile = "0";      // 0, 1, 2, 3
  int keyframe_interval = 120;    // Maximum distance between keyframes
  bool constant_bitrate = false;  // Use CBR instead of VBR
  int threads = 0;                // Number of threads (0 = auto)
  int tile_columns = 0;           // Log2 of number of tile columns (0-6)
  int tile_rows = 0;              // Log2 of number of tile rows (0-2)
};

struct AV1Params : public BaseCodecParams {
  int speed = 4;                  // Speed preset (0-10, lower is better quality)
  std::string profile = "main";   // main, high, professional
  int keyframe_interval = 120;    // Maximum distance between keyframes
  bool constant_bitrate = false;  // Use CBR instead of VBR
  int crf = 30;                   // Constant Rate Factor (quality, lower is better quality)
  int threads = 0;                // Number of threads (0 = auto)
  int tile_columns = 0;           // Number of tile columns (0=auto)
  int tile_rows = 0;              // Number of tile rows (0=auto)
};

} // namespace codec

// Generic video encoder configuration
struct VideoEncoderConfig {
  bool gpu_acceleration = false;  // Use GPU-accelerated encoding if available
  PixelFormat input_format = PixelFormat::YUV420;  // Input pixel format
  CodecType output_codec = CodecType::H264;        // Output codec type
  
  // Basic params common to all codecs
  int width = 1920;
  int height = 1080;
  int bitrate = 5000000;  // 5 Mbps
  int framerate = 30;
  
  // Advanced parameters specific to each codec
  std::unique_ptr<codec::BaseCodecParams> codec_params;
  
  // Helper methods to set specific codec parameters
  void SetH264Params(const codec::H264Params& params) {
    codec_params.reset(new codec::H264Params(params));
  }
  
  void SetHEVCParams(const codec::HEVCParams& params) {
    codec_params.reset(new codec::HEVCParams(params));
  }
  
  void SetVP8Params(const codec::VP8Params& params) {
    codec_params.reset(new codec::VP8Params(params));
  }
  
  void SetVP9Params(const codec::VP9Params& params) {
    codec_params.reset(new codec::VP9Params(params));
  }
  
  void SetAV1Params(const codec::AV1Params& params) {
    codec_params.reset(new codec::AV1Params(params));
  }
  
  // Default constructor
  VideoEncoderConfig() = default;
  
  // Copy constructor
  VideoEncoderConfig(const VideoEncoderConfig& other)
      : gpu_acceleration(other.gpu_acceleration),
        input_format(other.input_format),
        output_codec(other.output_codec),
        width(other.width),
        height(other.height),
        bitrate(other.bitrate),
        framerate(other.framerate) {
    // Copy codec_params if present
    if (other.codec_params) {
      // Copy based on codec type
      switch (output_codec) {
        case CodecType::H264:
          codec_params.reset(new codec::H264Params(
              *static_cast<const codec::H264Params*>(other.codec_params.get())));
          break;
        case CodecType::HEVC:
          codec_params.reset(new codec::HEVCParams(
              *static_cast<const codec::HEVCParams*>(other.codec_params.get())));
          break;
        case CodecType::VP8:
          codec_params.reset(new codec::VP8Params(
              *static_cast<const codec::VP8Params*>(other.codec_params.get())));
          break;
        case CodecType::VP9:
          codec_params.reset(new codec::VP9Params(
              *static_cast<const codec::VP9Params*>(other.codec_params.get())));
          break;
        case CodecType::AV1:
          codec_params.reset(new codec::AV1Params(
              *static_cast<const codec::AV1Params*>(other.codec_params.get())));
          break;
      }
    }
  }
  
  // Assignment operator
  VideoEncoderConfig& operator=(const VideoEncoderConfig& other) {
    if (this != &other) {
      gpu_acceleration = other.gpu_acceleration;
      input_format = other.input_format;
      output_codec = other.output_codec;
      width = other.width;
      height = other.height;
      bitrate = other.bitrate;
      framerate = other.framerate;
      
      // Copy codec_params if present
      if (other.codec_params) {
        // Copy based on codec type
        switch (output_codec) {
          case CodecType::H264:
            codec_params.reset(new codec::H264Params(
                *static_cast<const codec::H264Params*>(other.codec_params.get())));
            break;
          case CodecType::HEVC:
            codec_params.reset(new codec::HEVCParams(
                *static_cast<const codec::HEVCParams*>(other.codec_params.get())));
            break;
          case CodecType::VP8:
            codec_params.reset(new codec::VP8Params(
                *static_cast<const codec::VP8Params*>(other.codec_params.get())));
            break;
          case CodecType::VP9:
            codec_params.reset(new codec::VP9Params(
                *static_cast<const codec::VP9Params*>(other.codec_params.get())));
            break;
          case CodecType::AV1:
            codec_params.reset(new codec::AV1Params(
                *static_cast<const codec::AV1Params*>(other.codec_params.get())));
            break;
        }
      } else {
        codec_params.reset();
      }
    }
    return *this;
  }
};

// Video encoder interface
class VideoEncoder {
 public:
  // Factory method to create an encoder instance
  static std::unique_ptr<VideoEncoder> Create(const VideoEncoderConfig& config);
  
  // Virtual destructor
  virtual ~VideoEncoder() = default;
  
  // Encode a frame in YUV420 planar format
  virtual bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                           std::vector<uint8_t>* encoded_frame) = 0;
  
  // Encode a frame in NV12 semi-planar format (for GPU accelerated encoders)
  virtual bool EncodeNV12(const std::vector<uint8_t>& nv12_data,
                         std::vector<uint8_t>* encoded_frame);
  
  // Flush any buffered frames
  virtual bool Flush(std::vector<uint8_t>* encoded_frame);
  
  // Update encoder parameters at runtime
  virtual bool UpdateBitrate(int new_bitrate);
  virtual bool UpdateFramerate(int new_framerate);
  
  // Get current encoder configuration
  virtual VideoEncoderConfig GetConfig() const = 0;
};

} // namespace media

#endif  // MEDIA_VIDEO_ENCODER_H_