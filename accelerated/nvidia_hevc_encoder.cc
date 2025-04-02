#include "nvidia_hevc_encoder.h"

#include <string>
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}

namespace media {

std::unique_ptr<NvidiaHEVCEncoder> NvidiaHEVCEncoder::Create(
    const NvidiaHEVCEncoderConfig& config) {
  std::unique_ptr<NvidiaHEVCEncoder> encoder(new NvidiaHEVCEncoder(config));
  if (!encoder->Initialize(config)) {
    return nullptr;
  }
  return encoder;
}

NvidiaHEVCEncoder::NvidiaHEVCEncoder(const NvidiaHEVCEncoderConfig& config)
    : width_(config.width),
      height_(config.height),
      y_plane_size_(config.width * config.height),
      frame_size_(config.width * config.height * 3 / 2),
      pts_(0) {
  // Constructor only initializes simple members
  // Full initialization happens in Initialize()
}

NvidiaHEVCEncoder::~NvidiaHEVCEncoder() {
  if (packet_) {
    av_packet_free(&packet_);
  }
  
  if (frame_) {
    av_frame_free(&frame_);
  }
  
  if (codec_context_) {
    avcodec_free_context(&codec_context_);
  }
}

bool NvidiaHEVCEncoder::Initialize(const NvidiaHEVCEncoderConfig& config) {
  // Find the hevc_nvenc encoder
  const AVCodec* codec = avcodec_find_encoder_by_name("hevc_nvenc");
  if (!codec) {
    std::cerr << "NVIDIA HEVC encoder not found" << std::endl;
    return false;
  }
  
  // Create codec context
  codec_context_ = avcodec_alloc_context3(codec);
  if (!codec_context_) {
    std::cerr << "Failed to allocate encoder context" << std::endl;
    return false;
  }
  
  // Set basic encoding parameters
  codec_context_->width = config.width;
  codec_context_->height = config.height;
  codec_context_->time_base = AVRational{1, config.framerate};
  codec_context_->framerate = AVRational{config.framerate, 1};
  codec_context_->bit_rate = config.bitrate;
  codec_context_->gop_size = config.gop_length;
  codec_context_->max_b_frames = config.max_b_frames;
  
  // For NV12 and YUV420P input
  codec_context_->pix_fmt = AV_PIX_FMT_NV12;  // NVENC typically works with NV12 format
  
  // Set encoder-specific options using av_opt_set
  // Quality preset (1-7 in our API, but different in FFmpeg)
  const char* preset;
  switch (config.quality_preset) {
    case 1: preset = "slow"; break;
    case 2: preset = "medium"; break;
    case 3: preset = "fast"; break;
    case 4: preset = "fast"; break;  // Map 4 to "fast" as well
    case 5: preset = "hp"; break;    // High performance
    case 6: preset = "hq"; break;    // High quality
    case 7: preset = "bd"; break;    // Blu-ray quality
    default: preset = "medium"; break;
  }
  av_opt_set(codec_context_->priv_data, "preset", preset, 0);
  
  // Rate control mode: CBR or VBR
  if (config.use_cbr) {
    av_opt_set(codec_context_->priv_data, "rc", "cbr", 0);
  } else {
    av_opt_set(codec_context_->priv_data, "rc", "vbr", 0);
  }
  
  // Low-latency mode
  if (config.low_latency) {
    av_opt_set(codec_context_->priv_data, "delay", "0", 0);
    av_opt_set(codec_context_->priv_data, "zerolatency", "1", 0);
  }
  
  // Open the codec - avcodec will find the appropriate GPU device
  int ret = avcodec_open2(codec_context_, codec, nullptr);
  if (ret < 0) {
    char errbuf[128];
    av_strerror(ret, errbuf, sizeof(errbuf));
    std::cerr << "Failed to open codec: " << errbuf << std::endl;
    return false;
  }
  
  // Allocate frame and packet
  frame_ = av_frame_alloc();
  if (!frame_) {
    std::cerr << "Failed to allocate frame" << std::endl;
    return false;
  }
  
  // Set frame parameters
  frame_->format = codec_context_->pix_fmt;
  frame_->width = config.width;
  frame_->height = config.height;
  
  // Allocate the buffer for the frame
  ret = av_frame_get_buffer(frame_, 0);
  if (ret < 0) {
    std::cerr << "Failed to allocate frame buffer" << std::endl;
    return false;
  }
  
  packet_ = av_packet_alloc();
  if (!packet_) {
    std::cerr << "Failed to allocate packet" << std::endl;
    return false;
  }
  
  initialized_ = true;
  return true;
}

bool NvidiaHEVCEncoder::EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                                     std::vector<uint8_t>* encoded_frame) {
  if (!initialized_ || yuv_data.size() < frame_size_) {
    return false;
  }
  
  return EncodeFrame(yuv_data, encoded_frame, false);
}

bool NvidiaHEVCEncoder::EncodeNV12(const std::vector<uint8_t>& nv12_data,
                                   std::vector<uint8_t>* encoded_frame) {
  if (!initialized_ || nv12_data.size() < frame_size_) {
    return false;
  }
  
  return EncodeFrame(nv12_data, encoded_frame, true);
}

bool NvidiaHEVCEncoder::EncodeFrame(const std::vector<uint8_t>& frame_data,
                                   std::vector<uint8_t>* encoded_frame,
                                   bool is_nv12) {
  // Clear output buffer
  encoded_frame->clear();
  
  // Make sure the frame is writable
  int ret = av_frame_make_writable(frame_);
  if (ret < 0) {
    std::cerr << "Failed to make frame writable" << std::endl;
    return false;
  }
  
  // Copy frame data
  if (is_nv12) {
    // NV12: Y plane followed by interleaved UV plane
    std::memcpy(frame_->data[0], frame_data.data(), y_plane_size_);
    std::memcpy(frame_->data[1], frame_data.data() + y_plane_size_, y_plane_size_ / 2);
  } else {
    // YUV420P to NV12 conversion (if codec_context_->pix_fmt is NV12)
    if (codec_context_->pix_fmt == AV_PIX_FMT_NV12) {
      // Copy Y plane directly
      std::memcpy(frame_->data[0], frame_data.data(), y_plane_size_);
      
      // Convert separate U and V planes to interleaved UV plane
      const uint8_t* u_plane = frame_data.data() + y_plane_size_;
      const uint8_t* v_plane = frame_data.data() + y_plane_size_ + (y_plane_size_ / 4);
      uint8_t* uv_plane = frame_->data[1];
      
      for (int i = 0; i < y_plane_size_ / 4; i++) {
        uv_plane[i * 2] = u_plane[i];
        uv_plane[i * 2 + 1] = v_plane[i];
      }
    } else {
      // If the codec accepts YUV420P directly, just copy the planes
      std::memcpy(frame_->data[0], frame_data.data(), y_plane_size_);
      std::memcpy(frame_->data[1], frame_data.data() + y_plane_size_, y_plane_size_ / 4);
      std::memcpy(frame_->data[2], frame_data.data() + y_plane_size_ + (y_plane_size_ / 4), y_plane_size_ / 4);
    }
  }
  
  // Set timestamp
  frame_->pts = pts_++;
  
  // Send frame for encoding
  ret = avcodec_send_frame(codec_context_, frame_);
  if (ret < 0) {
    std::cerr << "Error sending frame for encoding: " << ret << std::endl;
    return false;
  }
  
  // Receive encoded packets
  ret = avcodec_receive_packet(codec_context_, packet_);
  if (ret < 0) {
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      // Need more data or end of stream, not necessarily an error
      return true;
    } else {
      std::cerr << "Error receiving packet from encoder: " << ret << std::endl;
      return false;
    }
  }
  
  // Copy the encoded data to output vector
  encoded_frame->resize(packet_->size);
  std::memcpy(encoded_frame->data(), packet_->data, packet_->size);
  
  // Unref packet for reuse
  av_packet_unref(packet_);
  
  return true;
}

}  // namespace media