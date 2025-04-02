#ifndef MEDIA_IMAGE_UTILS_H_
#define MEDIA_IMAGE_UTILS_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace media {

enum class ImageFormat {
  UNKNOWN,
  RGB,
  RGBA,
  BGRA,
  NV12,
  YUV420P
};

class ImageUtils {
 public:
  ImageUtils();
  ~ImageUtils();

  // Returns false if initialization failed
  explicit operator bool() const { return initialized_; }

  // Auto-detects input format and converts to NV12
  bool ConvertToNV12(const std::vector<uint8_t>& input_data, 
                     std::vector<uint8_t>& output_nv12,
                     int width = 0, 
                     int height = 0);

  // Auto-detects input format and converts to YUV420P
  bool ConvertToYUV420(const std::vector<uint8_t>& input_data, 
                       std::vector<uint8_t>& output_yuv420,
                       int width = 0, 
                       int height = 0);

  // Detects the image format of input data
  ImageFormat DetectFormat(const std::vector<uint8_t>& data, 
                           int width = 0, 
                           int height = 0);

  // If width and height are not provided, attempts to detect from data
  bool DetectDimensions(const std::vector<uint8_t>& data, 
                        ImageFormat format,
                        int& width, 
                        int& height);

 private:
  // Internal conversion method to prevent code duplication
  bool ConvertFormat(const std::vector<uint8_t>& input_data,
                     std::vector<uint8_t>& output_data,
                     ImageFormat target_format,
                     int width = 0,
                     int height = 0);

  bool initialized_;
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace media

#endif  // MEDIA_IMAGE_UTILS_H_