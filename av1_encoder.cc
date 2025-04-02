#include "av1_encoder.h"

#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

namespace media {

AV1Encoder::AV1Encoder() = default;

AV1Encoder::~AV1Encoder() {
  if (frame_) {
    av_frame_free(&frame_);
  }
  
  if (codec_context_) {
    avcodec_free_context(&codec_context_);
  }
}

std::unique_ptr<AV1Encoder> AV1Encoder::Create(const AV1EncoderConfig& config) {
  std::unique_ptr<AV1Encoder> encoder(new AV1Encoder());
  if (!encoder->Initialize(config)) {
    return nullptr;
  }
  return encoder;
}

bool AV1Encoder::Initialize(const AV1EncoderConfig& config) {
  config_ = config;

  // Find the AV1 encoder
  const AVCodec* codec = avcodec_find_encoder_by_name("libaom-av1");
  if (!codec) {
    std::cerr << "AV1 encoder not found" << std::endl;
    return false;
  }

  // Allocate codec context
  codec_context_ = avcodec_alloc_context3(codec);
  if (!codec_context_) {
    std::cerr << "Failed to allocate codec context" << std::endl;
    return false;
  }

  // Set basic encoding parameters
  codec_context_->width = config.width;
  codec_context_->height = config.height;
  codec_context_->time_base = AVRational{1, config.framerate};
  codec_context_->framerate = AVRational{config.framerate, 1};
  codec_context_->bit_rate = config.bitrate;
  codec_context_->gop_size = config.keyframe_interval;
  codec_context_->max_b_frames = 0;       // AV1 doesn't use B-frames
  codec_context_->pix_fmt = AV_PIX_FMT_YUV420P;
  codec_context_->thread_count = config.threads;

  // Set all advanced encoder parameters
  if (!SetEncoderParameters()) {
    return false;
  }

  // Open the codec
  int ret = avcodec_open2(codec_context_, codec, nullptr);
  if (ret < 0) {
    char error_buf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
    std::cerr << "Failed to open codec: " << error_buf << std::endl;
    return false;
  }

  // Allocate frame
  frame_ = av_frame_alloc();
  if (!frame_) {
    std::cerr << "Failed to allocate frame" << std::endl;
    return false;
  }

  frame_->format = codec_context_->pix_fmt;
  frame_->width = codec_context_->width;
  frame_->height = codec_context_->height;

  ret = av_frame_get_buffer(frame_, 0);
  if (ret < 0) {
    std::cerr << "Failed to allocate frame buffers" << std::endl;
    return false;
  }

  initialized_ = true;
  return true;
}

bool AV1Encoder::SetEncoderParameters() {
  if (!codec_context_ || !codec_context_->priv_data) {
    return false;
  }

  // Set speed preset (cpu-used in libaom-av1)
  int cpu_used = static_cast<int>(config_.speed_preset);
  av_opt_set_int(codec_context_->priv_data, "cpu-used", cpu_used, 0);

  // Set rate control mode
  switch (config_.rc_mode) {
    case AV1RateControlMode::CRF:
      av_opt_set_int(codec_context_->priv_data, "crf", config_.crf, 0);
      break;
    case AV1RateControlMode::CBR:
      av_opt_set(codec_context_->priv_data, "rate-control", "cbr", 0);
      break;
    case AV1RateControlMode::VBR:
      av_opt_set(codec_context_->priv_data, "rate-control", "vbr", 0);
      if (config_.vbr_target_percentage > 0) {
        av_opt_set_int(codec_context_->priv_data, "target-bitrate", 
                      (config_.bitrate * config_.vbr_target_percentage) / 100, 0);
      }
      break;
    case AV1RateControlMode::CQP:
      av_opt_set(codec_context_->priv_data, "rate-control", "q", 0);
      av_opt_set_int(codec_context_->priv_data, "qp", config_.qp, 0);
      break;
  }

  // Set min/max quantizer
  av_opt_set_int(codec_context_->priv_data, "qmin", config_.min_q, 0);
  av_opt_set_int(codec_context_->priv_data, "qmax", config_.max_q, 0);

  // Bitrate control
  av_opt_set_int(codec_context_->priv_data, "undershoot-pct", config_.bitrate_undershoot, 0);
  av_opt_set_int(codec_context_->priv_data, "overshoot-pct", config_.bitrate_overshoot, 0);

  // Tile configuration
  if (config_.tile_config == AV1TileConfig::SINGLE) {
    av_opt_set_int(codec_context_->priv_data, "tile-columns", 0, 0);
    av_opt_set_int(codec_context_->priv_data, "tile-rows", 0, 0);
  } else if (config_.tile_config == AV1TileConfig::MAXIMUM) {
    // Maximum tiles based on resolution
    int max_tile_cols = 6; // Max value for 4K video
    int max_tile_rows = 6; // Max value for 4K video
    av_opt_set_int(codec_context_->priv_data, "tile-columns", max_tile_cols, 0);
    av_opt_set_int(codec_context_->priv_data, "tile-rows", max_tile_rows, 0);
  } else if (config_.tile_columns > 0 || config_.tile_rows > 0) {
    // Use user-specified values
    av_opt_set_int(codec_context_->priv_data, "tile-columns", config_.tile_columns, 0);
    av_opt_set_int(codec_context_->priv_data, "tile-rows", config_.tile_rows, 0);
  }

  // Threading options
  av_opt_set_int(codec_context_->priv_data, "row-mt", config_.row_mt, 0);

  // GOP structure settings
  if (config_.max_intra_rate > 0) {
    av_opt_set_int(codec_context_->priv_data, "max-intra-rate", config_.max_intra_rate, 0);
  }
  
  if (config_.use_fixed_qp_offsets) {
    av_opt_set_int(codec_context_->priv_data, "delta-q-mode", 1, 0);
    av_opt_set_int(codec_context_->priv_data, "kf-delta-q", config_.keyframe_qp_offset, 0);
  }
  
  av_opt_set_int(codec_context_->priv_data, "max-reference-frames", config_.max_reference_frames, 0);

  // Visual quality parameters
  av_opt_set_int(codec_context_->priv_data, "arnr-strength", config_.arnr_strength, 0);
  av_opt_set_int(codec_context_->priv_data, "arnr-maxframes", config_.arnr_maxframes, 0);
  av_opt_set_int(codec_context_->priv_data, "enable-cdef", config_.enable_cdef ? 1 : 0, 0);
  av_opt_set_int(codec_context_->priv_data, "enable-restoration", config_.enable_restoration ? 1 : 0, 0);
  
  // Film grain parameters
  av_opt_set_int(codec_context_->priv_data, "enable-dnl-denoising", !config_.enable_film_grain, 0);
  if (config_.enable_film_grain) {
    av_opt_set_int(codec_context_->priv_data, "film-grain-denoise", 1, 0);
    av_opt_set_int(codec_context_->priv_data, "film-grain-strength", config_.film_grain_strength, 0);
  }
  
  // TPL model (look-ahead)
  av_opt_set_int(codec_context_->priv_data, "enable-tpl", config_.enable_tpl ? 1 : 0, 0);

  // Set color properties
  codec_context_->color_range = config_.color_range ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;

  // Partition settings
  av_opt_set_int(codec_context_->priv_data, "enable-rect-partitions", config_.enable_rect_partitions ? 1 : 0, 0);
  av_opt_set_int(codec_context_->priv_data, "enable-1to4-partitions", config_.enable_1to4_partitions ? 1 : 0, 0);
  av_opt_set_int(codec_context_->priv_data, "enable-cfl", config_.enable_cfl ? 1 : 0, 0);

  // Error resilience
  av_opt_set_int(codec_context_->priv_data, "error-resilient", config_.error_resilient_mode ? 1 : 0, 0);
  av_opt_set_int(codec_context_->priv_data, "frame-parallel", config_.frame_parallel_decoding ? 1 : 0, 0);

  // Content-based tuning
  if (config_.tune_content) {
    av_opt_set(codec_context_->priv_data, "tune-content", config_.content_type.c_str(), 0);
  }

  // Tune mode
  switch (config_.tune_mode) {
    case AV1TuneMode::PSNR:
      av_opt_set(codec_context_->priv_data, "tune", "psnr", 0);
      break;
    case AV1TuneMode::SSIM:
      av_opt_set(codec_context_->priv_data, "tune", "ssim", 0);
      break;
    case AV1TuneMode::VMAF:
      av_opt_set(codec_context_->priv_data, "tune", "vmaf", 0);
      break;
    case AV1TuneMode::FILM_GRAIN:
      av_opt_set(codec_context_->priv_data, "tune", "film_grain", 0);
      break;
    default:
      break;
  }

  return true;
}

bool AV1Encoder::EncodeYUV420(const std::vector<uint8_t>& yuv_data, 
                             std::vector<uint8_t>* output_frame) {
  if (!initialized_ || !output_frame) {
    return false;
  }

  // Make sure the frame is writable
  int ret = av_frame_make_writable(frame_);
  if (ret < 0) {
    std::cerr << "Failed to make frame writable" << std::endl;
    return false;
  }

  // Calculate plane sizes
  int y_size = config_.width * config_.height;
  int u_size = y_size / 4;
  int v_size = y_size / 4;

  // Check input size
  if (yuv_data.size() < static_cast<size_t>(y_size + u_size + v_size)) {
    std::cerr << "Input YUV data is too small" << std::endl;
    return false;
  }

  // Copy Y plane
  std::memcpy(frame_->data[0], yuv_data.data(), y_size);
  
  // Copy U plane
  std::memcpy(frame_->data[1], yuv_data.data() + y_size, u_size);
  
  // Copy V plane
  std::memcpy(frame_->data[2], yuv_data.data() + y_size + u_size, v_size);

  // Set presentation timestamp
  frame_->pts = pts_++;

  // Encode the frame
  ret = avcodec_send_frame(codec_context_, frame_);
  if (ret < 0) {
    std::cerr << "Error sending frame for encoding" << std::endl;
    return false;
  }

  // Get encoded packets
  AVPacket* packet = av_packet_alloc();
  if (!packet) {
    std::cerr << "Failed to allocate packet" << std::endl;
    return false;
  }

  bool success = false;
  ret = avcodec_receive_packet(codec_context_, packet);
  if (ret == 0) {
    success = ProcessEncodedPacket(packet, output_frame);
  } else if (ret == AVERROR(EAGAIN)) {
    // No output packet available yet, but not an error
    success = true;
    output_frame->clear();
  } else {
    std::cerr << "Error during encoding" << std::endl;
  }

  av_packet_free(&packet);
  return success;
}

bool AV1Encoder::Flush(std::vector<uint8_t>* output_frame) {
  if (!initialized_ || !output_frame) {
    return false;
  }

  // Signal end of stream
  int ret = avcodec_send_frame(codec_context_, nullptr);
  if (ret < 0) {
    std::cerr << "Error sending EOF" << std::endl;
    return false;
  }

  // Get remaining packets
  AVPacket* packet = av_packet_alloc();
  if (!packet) {
    std::cerr << "Failed to allocate packet" << std::endl;
    return false;
  }

  bool success = false;
  ret = avcodec_receive_packet(codec_context_, packet);
  if (ret == 0) {
    success = ProcessEncodedPacket(packet, output_frame);
  } else if (ret == AVERROR_EOF) {
    // End of stream, no more output packets
    success = true;
    output_frame->clear();
  } else {
    std::cerr << "Error during flushing" << std::endl;
  }

  av_packet_free(&packet);
  return success;
}

bool AV1Encoder::ProcessEncodedPacket(AVPacket* packet, std::vector<uint8_t>* output_frame) {
  if (!packet || !output_frame) {
    return false;
  }

  // Resize output vector and copy encoded data
  output_frame->resize(packet->size);
  std::memcpy(output_frame->data(), packet->data, packet->size);
  
  return true;
}

}  // namespace media