#include "image_utils.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace media {

class ImageUtils::Impl {
 public:
  Impl() : ctx_(nullptr) {}
  ~Impl() {
    if (ctx_) {
      sws_freeContext(ctx_);
      ctx_ = nullptr;
    }
  }

  SwsContext* ctx_;
};

ImageUtils::ImageUtils() : initialized_(false), impl_(std::make_unique<Impl>()) {
  // Initialize FFmpeg libraries if needed
  #if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
  #endif
  
  initialized_ = true;
}

ImageUtils::~ImageUtils() = default;

ImageFormat ImageUtils::DetectFormat(const std::vector<uint8_t>& data, int width, int height) {
  if (data.empty()) {
    return ImageFormat::UNKNOWN;
  }

  // Simple format detection based on header bytes
  // This is a basic implementation - in a real-world scenario, you'd want more robust detection
  
  // Check for common image file formats by examining header bytes
  if (data.size() >= 8) {
    // PNG signature
    if (data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
      return ImageFormat::RGBA;  // PNG typically uses RGBA
    }
    
    // JPEG signature
    if (data[0] == 0xFF && data[1] == 0xD8) {
      return ImageFormat::RGB;  // JPEG typically uses RGB
    }
  }
  
  // If no header detection and dimensions are provided, try to infer from data size
  if (width > 0 && height > 0 && !data.empty()) {
    size_t size = data.size();
    
    if (size == static_cast<size_t>(width * height * 3)) {
      return ImageFormat::RGB;
    } else if (size == static_cast<size_t>(width * height * 4)) {
      return ImageFormat::RGBA;  // Could be RGBA or BGRA, but we'll default to RGBA
    } else if (size == static_cast<size_t>(width * height * 3 / 2)) {
      // Both NV12 and YUV420P use 1.5 bytes per pixel, need more analysis
      // For simplicity, we'll check the pattern of the chroma planes
      
      // In YUV420P, U and V planes are separate
      // In NV12, U and V values are interleaved
      
      const size_t luma_size = width * height;
      const size_t chroma_offset = luma_size;
      
      // Check some chroma samples to determine if it's interleaved (NV12) or planar (YUV420P)
      bool interleaved = true;
      if (chroma_offset + 16 < data.size()) {
        // Check if chroma values follow interleaved pattern (alternating similar values)
        for (size_t i = 0; i < 8; i++) {
          if (std::abs(data[chroma_offset + i*2] - data[chroma_offset + i*2 + 1]) > 50) {
            interleaved = false;
            break;
          }
        }
      }
      
      return interleaved ? ImageFormat::NV12 : ImageFormat::YUV420P;
    }
  }
  
  return ImageFormat::UNKNOWN;
}

bool ImageUtils::DetectDimensions(const std::vector<uint8_t>& data, ImageFormat format, 
                                int& width, int& height) {
  // This is a simplified implementation - real-world code would need to parse
  // image headers to extract dimensions correctly
  
  if (width > 0 && height > 0) {
    // Dimensions already provided, validate against data size
    size_t expected_size = 0;
    
    switch (format) {
      case ImageFormat::RGB:
        expected_size = width * height * 3;
        break;
      case ImageFormat::RGBA:
      case ImageFormat::BGRA:
        expected_size = width * height * 4;
        break;
      case ImageFormat::NV12:
      case ImageFormat::YUV420P:
        expected_size = width * height * 3 / 2;
        break;
      default:
        return false;
    }
    
    return (data.size() >= expected_size);
  }
  
  // If dimensions are not provided, we could try to extract them from headers
  // For simplicity in this example, we'll return false
  return false;
}

bool ImageUtils::ConvertFormat(const std::vector<uint8_t>& input_data,
                             std::vector<uint8_t>& output_data,
                             ImageFormat target_format,
                             int width, int height) {
  if (!initialized_ || input_data.empty()) {
    return false;
  }
  
  // Detect input format if not explicitly provided
  ImageFormat src_format = DetectFormat(input_data, width, height);
  if (src_format == ImageFormat::UNKNOWN) {
    std::cerr << "Failed to detect input format" << std::endl;
    return false;
  }
  
  // Try to detect dimensions if not provided
  if (width <= 0 || height <= 0) {
    if (!DetectDimensions(input_data, src_format, width, height)) {
      std::cerr << "Failed to detect dimensions" << std::endl;
      return false;
    }
  }
  
  // Map formats to FFmpeg pixel formats
  AVPixelFormat src_pix_fmt;
  AVPixelFormat dst_pix_fmt;
  
  switch (src_format) {
    case ImageFormat::RGB:
      src_pix_fmt = AV_PIX_FMT_RGB24;
      break;
    case ImageFormat::RGBA:
      src_pix_fmt = AV_PIX_FMT_RGBA;
      break;
    case ImageFormat::BGRA:
      src_pix_fmt = AV_PIX_FMT_BGRA;
      break;
    case ImageFormat::NV12:
      src_pix_fmt = AV_PIX_FMT_NV12;
      break;
    case ImageFormat::YUV420P:
      src_pix_fmt = AV_PIX_FMT_YUV420P;
      break;
    default:
      std::cerr << "Unsupported source format" << std::endl;
      return false;
  }
  
  switch (target_format) {
    case ImageFormat::NV12:
      dst_pix_fmt = AV_PIX_FMT_NV12;
      break;
    case ImageFormat::YUV420P:
      dst_pix_fmt = AV_PIX_FMT_YUV420P;
      break;
    default:
      std::cerr << "Unsupported target format" << std::endl;
      return false;
  }
  
  // If formats are already the same, just copy the data
  if (src_format == target_format) {
    output_data = input_data;
    return true;
  }
  
  // Create scaling context
  impl_->ctx_ = sws_getCachedContext(
      impl_->ctx_,
      width, height, src_pix_fmt,
      width, height, dst_pix_fmt,
      SWS_BILINEAR, nullptr, nullptr, nullptr);
  
  if (!impl_->ctx_) {
    std::cerr << "Failed to create scaling context" << std::endl;
    return false;
  }
  
  // Allocate frame buffers
  AVFrame* src_frame = av_frame_alloc();
  AVFrame* dst_frame = av_frame_alloc();
  
  if (!src_frame || !dst_frame) {
    av_frame_free(&src_frame);
    av_frame_free(&dst_frame);
    std::cerr << "Failed to allocate frames" << std::endl;
    return false;
  }
  
  // Set up source frame
  src_frame->width = width;
  src_frame->height = height;
  src_frame->format = src_pix_fmt;
  
  int ret = av_image_alloc(
      src_frame->data, src_frame->linesize,
      width, height, src_pix_fmt, 1);
  
  if (ret < 0) {
    av_frame_free(&src_frame);
    av_frame_free(&dst_frame);
    std::cerr << "Failed to allocate source image" << std::endl;
    return false;
  }
  
  // Set up destination frame
  dst_frame->width = width;
  dst_frame->height = height;
  dst_frame->format = dst_pix_fmt;
  
  ret = av_image_alloc(
      dst_frame->data, dst_frame->linesize,
      width, height, dst_pix_fmt, 1);
  
  if (ret < 0) {
    av_freep(&src_frame->data[0]);
    av_frame_free(&src_frame);
    av_frame_free(&dst_frame);
    std::cerr << "Failed to allocate destination image" << std::endl;
    return false;
  }
  
  // Copy input data to source frame
  // This is a simplified approach - proper handling would depend on the actual data layout
  if (src_pix_fmt == AV_PIX_FMT_RGB24 || 
      src_pix_fmt == AV_PIX_FMT_RGBA || 
      src_pix_fmt == AV_PIX_FMT_BGRA) {
    // For packed formats (RGB, RGBA, BGRA), data is contiguous
    std::copy(input_data.begin(), input_data.end(), src_frame->data[0]);
  } else {
    // For planar formats (NV12, YUV420P), more careful copying is needed
    const size_t y_plane_size = width * height;
    
    // Copy Y plane
    std::copy(input_data.begin(), input_data.begin() + y_plane_size, src_frame->data[0]);
    
    if (src_pix_fmt == AV_PIX_FMT_NV12) {
      // For NV12, UV is interleaved in the second plane
      std::copy(input_data.begin() + y_plane_size, input_data.end(), src_frame->data[1]);
    } else if (src_pix_fmt == AV_PIX_FMT_YUV420P) {
      // For YUV420P, U and V are separate planes
      const size_t uv_plane_size = y_plane_size / 4;
      std::copy(input_data.begin() + y_plane_size, 
                input_data.begin() + y_plane_size + uv_plane_size, 
                src_frame->data[1]);  // U plane
      std::copy(input_data.begin() + y_plane_size + uv_plane_size, 
                input_data.end(), 
                src_frame->data[2]);  // V plane
    }
  }
  
  // Perform the conversion
  ret = sws_scale(impl_->ctx_,
                 src_frame->data, src_frame->linesize, 0, height,
                 dst_frame->data, dst_frame->linesize);
  
  if (ret <= 0) {
    av_freep(&src_frame->data[0]);
    av_freep(&dst_frame->data[0]);
    av_frame_free(&src_frame);
    av_frame_free(&dst_frame);
    std::cerr << "Scaling failed" << std::endl;
    return false;
  }
  
  // Calculate output size and prepare output buffer
  size_t output_size = 0;
  if (dst_pix_fmt == AV_PIX_FMT_NV12 || dst_pix_fmt == AV_PIX_FMT_YUV420P) {
    output_size = width * height * 3 / 2;  // Y + U/V at quarter size each
  } else {
    // This should not happen given our supported formats, but including for robustness
    av_freep(&src_frame->data[0]);
    av_freep(&dst_frame->data[0]);
    av_frame_free(&src_frame);
    av_frame_free(&dst_frame);
    std::cerr << "Unsupported output format for size calculation" << std::endl;
    return false;
  }
  
  output_data.resize(output_size);
  
  // Copy from destination frame to output buffer
  if (dst_pix_fmt == AV_PIX_FMT_NV12) {
    // For NV12, copy Y plane and interleaved UV plane
    const size_t y_plane_size = width * height;
    
    // Copy Y plane
    std::copy(dst_frame->data[0], 
              dst_frame->data[0] + y_plane_size, 
              output_data.begin());
    
    // Copy UV plane
    std::copy(dst_frame->data[1], 
              dst_frame->data[1] + (y_plane_size / 2), 
              output_data.begin() + y_plane_size);
  } else if (dst_pix_fmt == AV_PIX_FMT_YUV420P) {
    // For YUV420P, copy Y, U, and V planes separately
    const size_t y_plane_size = width * height;
    const size_t uv_plane_size = y_plane_size / 4;
    
    // Copy Y plane
    std::copy(dst_frame->data[0], 
              dst_frame->data[0] + y_plane_size, 
              output_data.begin());
    
    // Copy U plane
    std::copy(dst_frame->data[1], 
              dst_frame->data[1] + uv_plane_size, 
              output_data.begin() + y_plane_size);
    
    // Copy V plane
    std::copy(dst_frame->data[2], 
              dst_frame->data[2] + uv_plane_size, 
              output_data.begin() + y_plane_size + uv_plane_size);
  }
  
  // Clean up
  av_freep(&src_frame->data[0]);
  av_freep(&dst_frame->data[0]);
  av_frame_free(&src_frame);
  av_frame_free(&dst_frame);
  
  return true;
}

bool ImageUtils::ConvertToNV12(const std::vector<uint8_t>& input_data, 
                             std::vector<uint8_t>& output_nv12,
                             int width, int height) {
  return ConvertFormat(input_data, output_nv12, ImageFormat::NV12, width, height);
}

bool ImageUtils::ConvertToYUV420(const std::vector<uint8_t>& input_data, 
                               std::vector<uint8_t>& output_yuv420,
                               int width, int height) {
  return ConvertFormat(input_data, output_yuv420, ImageFormat::YUV420P, width, height);
}

}  // namespace media