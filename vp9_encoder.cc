#include "vp9_encoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <cstring>
#include <iostream>

namespace media {

namespace {

// Converts our VP9Quality enum to FFmpeg quality strings
const char* QualityToString(VP9Quality quality) {
  switch (quality) {
    case VP9Quality::REALTIME:
      return "realtime";
    case VP9Quality::BEST:
      return "best";
    case VP9Quality::GOOD:
    default:
      return "good";
  }
}

// Implementation of the VP9Encoder interface using libavcodec.
class VP9EncoderImpl : public VP9Encoder {
 public:
  // Create a VP9 encoder with the specified configuration.
  static std::unique_ptr<VP9EncoderImpl> Create(const VP9EncoderConfig& config);

  ~VP9EncoderImpl() override;

  // Encodes a raw YUV420 frame.
  bool EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                   std::vector<uint8_t>* encoded_frame) override;

  // Returns the current configuration of the encoder.
  const VP9EncoderConfig& GetConfig() const override { return config_; }
  
  // Update bitrate at runtime
  bool UpdateBitrate(int new_bitrate) override;
  
  // Update framerate at runtime
  bool UpdateFramerate(int new_framerate) override;

 private:
  VP9EncoderImpl(const VP9EncoderConfig& config,
                AVCodecContext* codec_context,
                AVFrame* frame,
                AVPacket* packet);

  // Configure codec context with all VP9-specific parameters
  bool ConfigureCodecContext(AVCodecContext* codec_context, const VP9EncoderConfig& config);

  // Configuration of the encoder.
  VP9EncoderConfig config_;

  // FFmpeg objects for encoding.
  AVCodecContext* codec_context_;
  AVFrame* frame_;
  AVPacket* packet_;
  
  // Track frame index for PTS
  int64_t frame_index_ = 0;
};

std::unique_ptr<VP9EncoderImpl> VP9EncoderImpl::Create(
    const VP9EncoderConfig& config) {
  // Find the VP9 encoder.
  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_VP9);
  if (!codec) {
    std::cerr << "VP9 codec not found" << std::endl;
    return nullptr;
  }

  // Allocate a codec context.
  AVCodecContext* codec_context = avcodec_alloc_context3(codec);
  if (!codec_context) {
    std::cerr << "Failed to allocate codec context" << std::endl;
    return nullptr;
  }

  // Set up base codec context parameters
  codec_context->width = config.width;
  codec_context->height = config.height;
  codec_context->time_base = AVRational{1, config.framerate};
  codec_context->framerate = AVRational{config.framerate, 1};
  codec_context->pix_fmt = AV_PIX_FMT_YUV420P;  // Default to YUV420P
  codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
  
  // Apply profile-specific settings
  if (config.profile == VP9Profile::PROFILE_1 || 
      config.profile == VP9Profile::PROFILE_3) {
    // For 4:2:2 or 4:4:4 profiles
    codec_context->pix_fmt = AV_PIX_FMT_YUV444P;
  }
  
  if (config.profile == VP9Profile::PROFILE_2 || 
      config.profile == VP9Profile::PROFILE_3) {
    // For 10-bit profiles
    if (config.bit_depth == 10) {
      if (config.profile == VP9Profile::PROFILE_2) {
        codec_context->pix_fmt = AV_PIX_FMT_YUV420P10LE;
      } else {
        codec_context->pix_fmt = AV_PIX_FMT_YUV444P10LE;
      }
    } 
    // For 12-bit profiles
    else if (config.bit_depth == 12) {
      if (config.profile == VP9Profile::PROFILE_2) {
        codec_context->pix_fmt = AV_PIX_FMT_YUV420P12LE;
      } else {
        codec_context->pix_fmt = AV_PIX_FMT_YUV444P12LE;
      }
    }
  }

  // Create an instance of our encoder
  auto encoder = std::unique_ptr<VP9EncoderImpl>(
      new VP9EncoderImpl(config, codec_context, nullptr, nullptr));
  
  // Configure the codec context with VP9-specific options
  if (!encoder->ConfigureCodecContext(codec_context, config)) {
    return nullptr;
  }

  // Open the codec.
  if (avcodec_open2(codec_context, codec, nullptr) < 0) {
    std::cerr << "Failed to open codec" << std::endl;
    avcodec_free_context(&codec_context);
    return nullptr;
  }

  // Allocate AVFrame
  AVFrame* frame = av_frame_alloc();
  if (!frame) {
    std::cerr << "Failed to allocate frame" << std::endl;
    avcodec_free_context(&codec_context);
    return nullptr;
  }

  frame->format = codec_context->pix_fmt;
  frame->width = codec_context->width;
  frame->height = codec_context->height;

  if (av_frame_get_buffer(frame, 0) < 0) {
    std::cerr << "Failed to allocate frame buffer" << std::endl;
    av_frame_free(&frame);
    avcodec_free_context(&codec_context);
    return nullptr;
  }

  // Allocate AVPacket
  AVPacket* packet = av_packet_alloc();
  if (!packet) {
    std::cerr << "Failed to allocate packet" << std::endl;
    av_frame_free(&frame);
    avcodec_free_context(&codec_context);
    return nullptr;
  }

  // Update the frame and packet pointers in our encoder
  encoder->frame_ = frame;
  encoder->packet_ = packet;
  
  return encoder;
}

bool VP9EncoderImpl::ConfigureCodecContext(AVCodecContext* codec_context, 
                                          const VP9EncoderConfig& config) {
  if (!codec_context) {
    return false;
  }
  
  // Bitrate settings
  codec_context->bit_rate = config.bitrate;
  
  if (config.use_cbr) {
    av_opt_set(codec_context->priv_data, "rc_mode", "CBR", 0);
  } else {
    av_opt_set(codec_context->priv_data, "rc_mode", "VBR", 0);
  }
  
  if (config.max_bitrate > 0) {
    codec_context->rc_max_rate = config.max_bitrate;
  }
  
  if (config.min_bitrate > 0) {
    codec_context->rc_min_rate = config.min_bitrate;
  }
  
  if (config.buffer_size > 0) {
    codec_context->rc_buffer_size = config.buffer_size;
  }
  
  if (config.buffer_initial_size > 0) {
    codec_context->rc_initial_buffer_occupancy = config.buffer_initial_size;
  }
  
  // Quality settings
  av_opt_set(codec_context->priv_data, "quality", 
             QualityToString(config.quality), 0);
  
  if (config.crf > 0) {
    // In libavcodec, VP9 uses crf for constant quality mode
    av_opt_set_int(codec_context->priv_data, "crf", config.crf, 0);
    av_opt_set(codec_context->priv_data, "rc_mode", "CQ", 0);
  }
  
  av_opt_set_int(codec_context->priv_data, "speed", config.speed, 0);
  
  if (config.lossless) {
    av_opt_set_int(codec_context->priv_data, "lossless", 1, 0);
  }
  
  // GOP structure
  codec_context->gop_size = config.keyframe_interval;
  
  if (config.auto_alt_ref) {
    av_opt_set_int(codec_context->priv_data, "auto-alt-ref", 1, 0);
  } else {
    av_opt_set_int(codec_context->priv_data, "auto-alt-ref", 0, 0);
  }
  
  if (config.lag_in_frames > 0) {
    codec_context->delay = config.lag_in_frames;
    av_opt_set_int(codec_context->priv_data, "lag-in-frames", 
                   config.lag_in_frames, 0);
  }
  
  // Threading settings
  if (config.tile_columns > 0) {
    av_opt_set_int(codec_context->priv_data, "tile-columns", 
                  config.tile_columns, 0);
  }
  
  if (config.tile_rows > 0) {
    av_opt_set_int(codec_context->priv_data, "tile-rows", 
                  config.tile_rows, 0);
  }
  
  if (config.frame_parallel) {
    av_opt_set_int(codec_context->priv_data, "frame-parallel", 1, 0);
  }
  
  if (config.threads > 0) {
    codec_context->thread_count = config.threads;
  }
  
  // Visual quality tuning
  if (config.error_resilient) {
    codec_context->flags |= AV_CODEC_FLAG_PSNR;
    av_opt_set_int(codec_context->priv_data, "error-resilient", 1, 0);
  }
  
  if (config.arnr_enabled) {
    av_opt_set_int(codec_context->priv_data, "arnr", 1, 0);
    av_opt_set_int(codec_context->priv_data, "arnr-strength", 
                  config.arnr_strength, 0);
    av_opt_set_int(codec_context->priv_data, "arnr-maxframes", 
                  config.arnr_max_frames, 0);
  }
  
  // Profile settings
  codec_context->profile = static_cast<int>(config.profile);
  
  // ROI (Region of Interest) settings
  if (config.roi_enabled) {
    av_opt_set_int(codec_context->priv_data, "roi_map", 1, 0);
  }
  
  // SVC (Scalable Video Coding) settings
  if (config.svc_enabled) {
    av_opt_set_int(codec_context->priv_data, "svc", 1, 0);
    
    char svc_params[64];
    snprintf(svc_params, sizeof(svc_params), "l%dt%d", 
             config.svc_layers, config.svc_temporal_layers);
    av_opt_set(codec_context->priv_data, "svc_params", svc_params, 0);
  }
  
  // Advanced tuning parameters
  if (config.aq_mode) {
    av_opt_set_int(codec_context->priv_data, "aq-mode", 1, 0);
  }
  
  if (config.undershoot_pct != 100) {
    codec_context->rc_min_rate = config.bitrate * config.undershoot_pct / 100;
  }
  
  if (config.overshoot_pct != 100) {
    codec_context->rc_max_rate = config.bitrate * config.overshoot_pct / 100;
  }
  
  if (config.max_intra_bitrate_pct > 0) {
    av_opt_set_int(codec_context->priv_data, "maxrate", 
                  config.bitrate * config.max_intra_bitrate_pct / 100, 0);
  }
  
  if (config.row_mt) {
    av_opt_set_int(codec_context->priv_data, "row-mt", 1, 0);
  }
  
  return true;
}

VP9EncoderImpl::VP9EncoderImpl(const VP9EncoderConfig& config,
                              AVCodecContext* codec_context,
                              AVFrame* frame,
                              AVPacket* packet)
    : config_(config),
      codec_context_(codec_context),
      frame_(frame),
      packet_(packet),
      frame_index_(0) {}

VP9EncoderImpl::~VP9EncoderImpl() {
  // Flush the encoder
  avcodec_send_frame(codec_context_, nullptr);
  
  while (avcodec_receive_packet(codec_context_, packet_) >= 0) {
    av_packet_unref(packet_);
  }
  
  av_packet_free(&packet_);
  av_frame_free(&frame_);
  avcodec_free_context(&codec_context_);
}

bool VP9EncoderImpl::EncodeYUV420(const std::vector<uint8_t>& yuv_data,
                                 std::vector<uint8_t>* encoded_frame) {
  if (yuv_data.size() < (config_.width * config_.height * 3) / 2) {
    std::cerr << "YUV data size too small for the specified resolution" << std::endl;
    return false;
  }

  if (!encoded_frame) {
    std::cerr << "Output buffer pointer is null" << std::endl;
    return false;
  }

  // Make sure the frame is writable
  if (av_frame_make_writable(frame_) < 0) {
    std::cerr << "Failed to make frame writable" << std::endl;
    return false;
  }

  // Copy Y plane
  const size_t y_plane_size = config_.width * config_.height;
  std::memcpy(frame_->data[0], yuv_data.data(), y_plane_size);

  // Copy U plane
  const size_t u_plane_size = (config_.width * config_.height) / 4;
  std::memcpy(frame_->data[1], yuv_data.data() + y_plane_size, u_plane_size);

  // Copy V plane
  const size_t v_plane_offset = y_plane_size + u_plane_size;
  std::memcpy(frame_->data[2], yuv_data.data() + v_plane_offset, u_plane_size);

  // Set the presentation timestamp
  frame_->pts = frame_index_++;

  // Send the frame to the encoder
  int ret = avcodec_send_frame(codec_context_, frame_);
  if (ret < 0) {
    std::cerr << "Error sending frame to encoder: " << ret << std::endl;
    return false;
  }

  // Get encoded packets
  ret = avcodec_receive_packet(codec_context_, packet_);
  if (ret < 0) {
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      // EAGAIN means we need to feed more frames
      // EOF means the encoder is flushed
      // Both are not actual errors in this context
      return true;
    }
    std::cerr << "Error receiving packet from encoder: " << ret << std::endl;
    return false;
  }

  // Copy encoded data to output buffer
  encoded_frame->resize(packet_->size);
  std::memcpy(encoded_frame->data(), packet_->data, packet_->size);

  // Unref the packet for reuse
  av_packet_unref(packet_);

  return true;
}

bool VP9EncoderImpl::UpdateBitrate(int new_bitrate) {
  if (new_bitrate <= 0) {
    return false;
  }
  
  // Update the bitrate in our configuration
  config_.bitrate = new_bitrate;
  
  // Update the codec context
  codec_context_->bit_rate = new_bitrate;
  
  // If we have min/max rates set, update them proportionally
  if (config_.min_bitrate > 0) {
    codec_context_->rc_min_rate = 
        new_bitrate * config_.undershoot_pct / 100;
  }
  
  if (config_.max_bitrate > 0) {
    codec_context_->rc_max_rate = 
        new_bitrate * config_.overshoot_pct / 100;
  }
  
  return true;
}

bool VP9EncoderImpl::UpdateFramerate(int new_framerate) {
  if (new_framerate <= 0) {
    return false;
  }
  
  // Update the framerate in our configuration
  config_.framerate = new_framerate;
  
  // Update the codec context
  codec_context_->time_base = (AVRational){1, new_framerate};
  codec_context_->framerate = (AVRational){new_framerate, 1};
  
  return true;
}

}  // namespace

std::unique_ptr<VP9Encoder> VP9Encoder::Create(const VP9EncoderConfig& config) {
  return VP9EncoderImpl::Create(config);
}

}  // namespace media