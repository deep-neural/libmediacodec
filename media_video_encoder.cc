#include "media_video_encoder.h"

#include <cstring>
#include <iostream>
#include <string>
#include <typeinfo>

#include "h264_encoder.h"
#include "hevc_encoder.h"
#include "vp8_encoder.h"
#include "vp9_encoder.h"
#include "av1_encoder.h"


#include "nvidia_h264_encoder.h"
#include "nvidia_hevc_encoder.h"
#include "nvidia_av1_encoder.h"


namespace media {

// Default implementation for methods that are not always required
bool VideoEncoder::EncodeNV12(const std::vector<uint8_t>& nv12_data,
                             std::vector<uint8_t>* encoded_frame) {
  // Default implementation: not supported
  std::cerr << "EncodeNV12 is not supported by this encoder" << std::endl;
  return false;
}

bool VideoEncoder::Flush(std::vector<uint8_t>* encoded_frame) {
  // Default implementation: nothing to flush
  return true;
}

bool VideoEncoder::UpdateBitrate(int new_bitrate) {
  // Default implementation: not supported
  std::cerr << "UpdateBitrate is not supported by this encoder" << std::endl;
  return false;
}

bool VideoEncoder::UpdateFramerate(int new_framerate) {
  // Default implementation: not supported
  std::cerr << "UpdateFramerate is not supported by this encoder" << std::endl;
  return false;
}

namespace {

// Helper class for H264 encoder implementation
class H264EncoderImpl : public VideoEncoder {
 public:
  explicit H264EncoderImpl(const VideoEncoderConfig& config) 
      : config_(config) {
    h264_config_.width = config.width;
    h264_config_.height = config.height;
    h264_config_.bitrate = config.bitrate;
    h264_config_.framerate = config.framerate;
    
    // Apply advanced parameters if provided
    if (config.codec_params && typeid(*config.codec_params) == typeid(codec::H264Params)) {
      const auto& advanced = *static_cast<const codec::H264Params*>(config.codec_params.get());
      h264_config_.preset = advanced.preset;
      h264_config_.profile = advanced.profile;
      h264_config_.level = advanced.level;
      h264_config_.gop_size = advanced.keyframe_interval;
      h264_config_.max_b_frames = advanced.max_b_frames;
      h264_config_.constant_bitrate = advanced.constant_bitrate;
      h264_config_.crf = advanced.crf;
      h264_config_.threads = advanced.threads;
    }
    
    encoder_ = H264Encoder::Create(h264_config_);
  }
  
  bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                   std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeYUV420(yuv_data, encoded_frame);
  }
  
  bool Flush(std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->Flush(encoded_frame);
  }
  
  bool UpdateBitrate(int new_bitrate) override {
    if (!encoder_) return false;
    H264EncoderConfig new_config = h264_config_;
    new_config.bitrate = new_bitrate;
    return encoder_->Reconfigure(new_config);
  }
  
  VideoEncoderConfig GetConfig() const override {
    return config_;
  }
  
 private:
  VideoEncoderConfig config_;
  H264EncoderConfig h264_config_;
  std::unique_ptr<H264Encoder> encoder_;
};

// Helper class for HEVC encoder implementation
class HEVCEncoderImpl : public VideoEncoder {
 public:
  explicit HEVCEncoderImpl(const VideoEncoderConfig& config) 
      : config_(config) {
    hevc_config_.width = config.width;
    hevc_config_.height = config.height;
    hevc_config_.bitrate = config.bitrate;
    hevc_config_.framerate = config.framerate;
    
    // Apply advanced parameters if provided
    if (config.codec_params && typeid(*config.codec_params) == typeid(codec::HEVCParams)) {
      const auto& advanced = *static_cast<const codec::HEVCParams*>(config.codec_params.get());
      
      // Map preset string to enum
      if (advanced.preset == "ultrafast") hevc_config_.preset = HEVCPreset::ULTRAFAST;
      else if (advanced.preset == "superfast") hevc_config_.preset = HEVCPreset::SUPERFAST;
      else if (advanced.preset == "veryfast") hevc_config_.preset = HEVCPreset::VERYFAST;
      else if (advanced.preset == "faster") hevc_config_.preset = HEVCPreset::FASTER;
      else if (advanced.preset == "fast") hevc_config_.preset = HEVCPreset::FAST;
      else if (advanced.preset == "medium") hevc_config_.preset = HEVCPreset::MEDIUM;
      else if (advanced.preset == "slow") hevc_config_.preset = HEVCPreset::SLOW;
      else if (advanced.preset == "slower") hevc_config_.preset = HEVCPreset::SLOWER;
      else if (advanced.preset == "veryslow") hevc_config_.preset = HEVCPreset::VERYSLOW;
      else if (advanced.preset == "placebo") hevc_config_.preset = HEVCPreset::PLACEBO;
      
      // Map profile string to enum
      if (advanced.profile == "main") hevc_config_.profile = HEVCProfile::MAIN;
      else if (advanced.profile == "main10") hevc_config_.profile = HEVCProfile::MAIN_10;
      else if (advanced.profile == "main-still-picture") hevc_config_.profile = HEVCProfile::MAIN_STILL_PICTURE;
      else if (advanced.profile == "rext") hevc_config_.profile = HEVCProfile::REXT;
      
      // Apply other settings
      hevc_config_.keyint_max = advanced.keyframe_interval;
      hevc_config_.crf = advanced.crf;
      hevc_config_.rc_mode = advanced.constant_bitrate ? RateControlMode::CBR : RateControlMode::CRF;
      hevc_config_.bframes = advanced.max_b_frames;
      hevc_config_.threads = advanced.threads;
    }
    
    encoder_ = HEVCEncoder::Create(hevc_config_);
  }
  
  bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                   std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeYUV420(yuv_data, encoded_frame) == 1;
  }
  
  bool Flush(std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->Flush(encoded_frame) == 1;
  }
  
  bool UpdateParams(int new_bitrate, int new_framerate) {
    if (!encoder_) return false;
    return encoder_->UpdateParams(new_bitrate, new_framerate);
  }
  
  bool UpdateBitrate(int new_bitrate) override {
    return UpdateParams(new_bitrate, hevc_config_.framerate);
  }
  
  bool UpdateFramerate(int new_framerate) override {
    return UpdateParams(hevc_config_.bitrate, new_framerate);
  }
  
  VideoEncoderConfig GetConfig() const override {
    return config_;
  }
  
 private:
  VideoEncoderConfig config_;
  HEVCEncoderConfig hevc_config_;
  std::unique_ptr<HEVCEncoder> encoder_;
};

// Helper class for VP8 encoder implementation
class VP8EncoderImpl : public VideoEncoder {
 public:
  explicit VP8EncoderImpl(const VideoEncoderConfig& config) 
      : config_(config) {
    vp8_config_.width = config.width;
    vp8_config_.height = config.height;
    vp8_config_.bitrate = config.bitrate;
    vp8_config_.framerate = config.framerate;
    
    // Apply advanced parameters if provided
    if (config.codec_params && typeid(*config.codec_params) == typeid(codec::VP8Params)) {
      const auto& advanced = *static_cast<const codec::VP8Params*>(config.codec_params.get());
      vp8_config_.quality = advanced.quality;
      vp8_config_.keyframe_interval = advanced.keyframe_interval;
      vp8_config_.rc_mode = advanced.constant_bitrate ? VP8EncoderConfig::RC_MODE_CBR : VP8EncoderConfig::RC_MODE_VBR;
      vp8_config_.thread_count = advanced.threads;
    }
    
    encoder_.reset(VP8Encoder::Create(vp8_config_));
  }
  
  bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                   std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeYUV420(yuv_data, encoded_frame) > 0;
  }
  
  VideoEncoderConfig GetConfig() const override {
    return config_;
  }
  
 private:
  VideoEncoderConfig config_;
  VP8EncoderConfig vp8_config_;
  std::unique_ptr<VP8Encoder> encoder_;
};

// Helper class for VP9 encoder implementation
class VP9EncoderImpl : public VideoEncoder {
 public:
  explicit VP9EncoderImpl(const VideoEncoderConfig& config) 
      : config_(config) {
    vp9_config_.width = config.width;
    vp9_config_.height = config.height;
    vp9_config_.bitrate = config.bitrate;
    vp9_config_.framerate = config.framerate;
    
    // Apply advanced parameters if provided
    if (config.codec_params && typeid(*config.codec_params) == typeid(codec::VP9Params)) {
      const auto& advanced = *static_cast<const codec::VP9Params*>(config.codec_params.get());
      vp9_config_.crf = advanced.quality;
      vp9_config_.keyframe_interval = advanced.keyframe_interval;
      vp9_config_.use_cbr = advanced.constant_bitrate;
      vp9_config_.threads = advanced.threads;
      vp9_config_.tile_columns = advanced.tile_columns;
      vp9_config_.tile_rows = advanced.tile_rows;
      
      // Map quality string to enum
      if (advanced.speed == "realtime") vp9_config_.quality = VP9Quality::REALTIME;
      else if (advanced.speed == "good") vp9_config_.quality = VP9Quality::GOOD;
      else if (advanced.speed == "best") vp9_config_.quality = VP9Quality::BEST;
      
      // Map profile string to enum
      if (advanced.profile == "0") vp9_config_.profile = VP9Profile::PROFILE_0;
      else if (advanced.profile == "1") vp9_config_.profile = VP9Profile::PROFILE_1;
      else if (advanced.profile == "2") vp9_config_.profile = VP9Profile::PROFILE_2;
      else if (advanced.profile == "3") vp9_config_.profile = VP9Profile::PROFILE_3;
    }
    
    encoder_ = VP9Encoder::Create(vp9_config_);
  }
  
  bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                   std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeYUV420(yuv_data, encoded_frame);
  }
  
  bool UpdateBitrate(int new_bitrate) override {
    if (!encoder_) return false;
    return encoder_->UpdateBitrate(new_bitrate);
  }
  
  bool UpdateFramerate(int new_framerate) override {
    if (!encoder_) return false;
    return encoder_->UpdateFramerate(new_framerate);
  }
  
  VideoEncoderConfig GetConfig() const override {
    return config_;
  }
  
 private:
  VideoEncoderConfig config_;
  VP9EncoderConfig vp9_config_;
  std::unique_ptr<VP9Encoder> encoder_;
};

// Helper class for AV1 encoder implementation
class AV1EncoderImpl : public VideoEncoder {
 public:
  explicit AV1EncoderImpl(const VideoEncoderConfig& config) 
      : config_(config) {
    av1_config_.width = config.width;
    av1_config_.height = config.height;
    av1_config_.bitrate = config.bitrate;
    av1_config_.framerate = config.framerate;
    
    // Apply advanced parameters if provided
    if (config.codec_params && typeid(*config.codec_params) == typeid(codec::AV1Params)) {
      const auto& advanced = *static_cast<const codec::AV1Params*>(config.codec_params.get());
      
      // Map speed to appropriate enum value (0-10)
      if (advanced.speed >= 0 && advanced.speed <= 10) {
        switch (advanced.speed) {
          case 0: av1_config_.speed_preset = AV1SpeedPreset::SLOWEST; break;
          case 1: av1_config_.speed_preset = AV1SpeedPreset::SLOWER; break;
          case 2: av1_config_.speed_preset = AV1SpeedPreset::SLOW; break;
          case 3:
          case 4: av1_config_.speed_preset = AV1SpeedPreset::MEDIUM; break;
          case 5:
          case 6: av1_config_.speed_preset = AV1SpeedPreset::FAST; break;
          case 7:
          case 8: av1_config_.speed_preset = AV1SpeedPreset::FASTER; break;
          case 9:
          case 10: av1_config_.speed_preset = AV1SpeedPreset::FASTEST; break;
        }
      }
      
      av1_config_.keyframe_interval = advanced.keyframe_interval;
      av1_config_.rc_mode = advanced.constant_bitrate ? 
          AV1RateControlMode::CBR : AV1RateControlMode::CRF;
      av1_config_.crf = advanced.crf;
      av1_config_.threads = advanced.threads;
      av1_config_.tile_columns = advanced.tile_columns;
      av1_config_.tile_rows = advanced.tile_rows;
    }
    
    encoder_ = AV1Encoder::Create(av1_config_);
  }
  
  bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                   std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeYUV420(yuv_data, encoded_frame);
  }
  
  bool Flush(std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->Flush(encoded_frame);
  }
  
  VideoEncoderConfig GetConfig() const override {
    return config_;
  }
  
 private:
  VideoEncoderConfig config_;
  AV1EncoderConfig av1_config_;
  std::unique_ptr<AV1Encoder> encoder_;
};


// Helper class for NVIDIA H264 encoder implementation
class NvidiaH264EncoderImpl : public VideoEncoder {
 public:
  explicit NvidiaH264EncoderImpl(const VideoEncoderConfig& config) 
      : config_(config) {
    nvidia_config_.width = config.width;
    nvidia_config_.height = config.height;
    nvidia_config_.bitrate = config.bitrate;
    nvidia_config_.framerate = config.framerate;
    
    // Apply advanced parameters if provided
    if (config.codec_params && typeid(*config.codec_params) == typeid(codec::H264Params)) {
      const auto& advanced = *static_cast<const codec::H264Params*>(config.codec_params.get());
      nvidia_config_.gop_length = advanced.keyframe_interval;
      nvidia_config_.max_b_frames = advanced.max_b_frames;
      nvidia_config_.use_cbr = advanced.constant_bitrate;
    }
    
    encoder_ = NvidiaH264Encoder::Create(nvidia_config_);
  }
  
  bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                   std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeYUV420(yuv_data, encoded_frame);
  }
  
  bool EncodeNV12(const std::vector<uint8_t>& nv12_data,
                 std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeNV12(nv12_data, encoded_frame);
  }
  
  VideoEncoderConfig GetConfig() const override {
    return config_;
  }
  
 private:
  VideoEncoderConfig config_;
  NvidiaH264EncoderConfig nvidia_config_;
  std::unique_ptr<NvidiaH264Encoder> encoder_;
};

// Helper class for NVIDIA HEVC encoder implementation
class NvidiaHEVCEncoderImpl : public VideoEncoder {
 public:
  explicit NvidiaHEVCEncoderImpl(const VideoEncoderConfig& config) 
      : config_(config) {
    nvidia_config_.width = config.width;
    nvidia_config_.height = config.height;
    nvidia_config_.bitrate = config.bitrate;
    nvidia_config_.framerate = config.framerate;
    
    // Apply advanced parameters if provided
    if (config.codec_params && typeid(*config.codec_params) == typeid(codec::HEVCParams)) {
      const auto& advanced = *static_cast<const codec::HEVCParams*>(config.codec_params.get());
      nvidia_config_.gop_length = advanced.keyframe_interval;
      nvidia_config_.max_b_frames = advanced.max_b_frames;
      nvidia_config_.use_cbr = advanced.constant_bitrate;
    }
    
    encoder_ = NvidiaHEVCEncoder::Create(nvidia_config_);
  }
  
  bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                   std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeYUV420(yuv_data, encoded_frame);
  }
  
  bool EncodeNV12(const std::vector<uint8_t>& nv12_data,
                 std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeNV12(nv12_data, encoded_frame);
  }
  
  VideoEncoderConfig GetConfig() const override {
    return config_;
  }
  
 private:
  VideoEncoderConfig config_;
  NvidiaHEVCEncoderConfig nvidia_config_;
  std::unique_ptr<NvidiaHEVCEncoder> encoder_;
};

// Helper class for NVIDIA AV1 encoder implementation
class NvidiaAV1EncoderImpl : public VideoEncoder {
 public:
  explicit NvidiaAV1EncoderImpl(const VideoEncoderConfig& config) 
      : config_(config) {
    nvidia_config_.width = config.width;
    nvidia_config_.height = config.height;
    nvidia_config_.bitrate = config.bitrate;
    nvidia_config_.framerate = config.framerate;
    
    // Apply advanced parameters if provided
    if (config.codec_params && typeid(*config.codec_params) == typeid(codec::AV1Params)) {
      const auto& advanced = *static_cast<const codec::AV1Params*>(config.codec_params.get());
      nvidia_config_.gop_length = advanced.keyframe_interval;
      nvidia_config_.use_cbr = advanced.constant_bitrate;
      nvidia_config_.tile_columns = advanced.tile_columns;
      nvidia_config_.tile_rows = advanced.tile_rows;
    }
    
    encoder_ = NvidiaAV1Encoder::Create(nvidia_config_);
  }
  
  bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                   std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeYUV420(yuv_data, encoded_frame);
  }
  
  bool EncodeNV12(const std::vector<uint8_t>& nv12_data,
                 std::vector<uint8_t>* encoded_frame) override {
    if (!encoder_) return false;
    return encoder_->EncodeNV12(nv12_data, encoded_frame);
  }
  
  VideoEncoderConfig GetConfig() const override {
    return config_;
  }
  
 private:
  VideoEncoderConfig config_;
  NvidiaAV1EncoderConfig nvidia_config_;
  std::unique_ptr<NvidiaAV1Encoder> encoder_;
};


} // namespace

std::unique_ptr<VideoEncoder> VideoEncoder::Create(const VideoEncoderConfig& config) {
  const auto codec = config.output_codec;
  const bool use_gpu = config.gpu_acceleration;
  
  // For GPU-accelerated encoders
  if (use_gpu) {

    switch (codec) {
      case CodecType::H264:
        return std::unique_ptr<VideoEncoder>(new NvidiaH264EncoderImpl(config));
      case CodecType::HEVC:
        return std::unique_ptr<VideoEncoder>(new NvidiaHEVCEncoderImpl(config));
      case CodecType::AV1:
        return std::unique_ptr<VideoEncoder>(new NvidiaAV1EncoderImpl(config));
      default:
        std::cerr << "GPU acceleration not supported for this codec. "
                  << "Falling back to CPU encoder." << std::endl;
        // Fall through to software encoders
    }

    std::cerr << "GPU acceleration requested but not available. "
              << "Falling back to CPU encoder." << std::endl;
    // Fall through to software encoders

  }
  
  // Software encoders
  switch (codec) {
    case CodecType::H264:
      return std::unique_ptr<VideoEncoder>(new H264EncoderImpl(config));
    case CodecType::HEVC:
      return std::unique_ptr<VideoEncoder>(new HEVCEncoderImpl(config));
    case CodecType::VP8:
      return std::unique_ptr<VideoEncoder>(new VP8EncoderImpl(config));
    case CodecType::VP9:
      return std::unique_ptr<VideoEncoder>(new VP9EncoderImpl(config));
    case CodecType::AV1:
      return std::unique_ptr<VideoEncoder>(new AV1EncoderImpl(config));
    default:
      std::cerr << "Unsupported codec type" << std::endl;
      return nullptr;
  }
}

} // namespace media