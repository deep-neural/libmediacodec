#include "hevc_decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
}

#include <cstring>
#include <iostream>
#include <sstream>

namespace media {

namespace {

class HEVCDecoderImpl : public HEVCDecoder {
 public:
  explicit HEVCDecoderImpl(const HEVCDecoderConfig& config);
  ~HEVCDecoderImpl() override;

  // Initialize the decoder
  bool Initialize();

  // Implementation of HEVCDecoder interface
  int DecodeToYUV420(std::vector<uint8_t>* yuv_frame,
                    const std::vector<uint8_t>* hevc_frame) override;
  int GetWidth() const override;
  int GetHeight() const override;
  void Flush() override;
  void Reset() override;
  bool UpdateConfig(const HEVCDecoderConfig& config) override;
  HEVCDecoderConfig GetConfig() const override;

 private:
  // Free allocated resources
  void Cleanup();
  
  // Apply config to codec context
  bool ApplyConfig();

  // FFmpeg structures
  const AVCodec* codec_ = nullptr;
  AVCodecContext* codec_ctx_ = nullptr;
  AVFrame* av_frame_ = nullptr;
  AVPacket* av_packet_ = nullptr;

  // Configuration parameters
  HEVCDecoderConfig config_;

  // Flag to track if decoder is initialized
  bool initialized_ = false;
};

HEVCDecoderImpl::HEVCDecoderImpl(const HEVCDecoderConfig& config)
    : config_(config) {}

HEVCDecoderImpl::~HEVCDecoderImpl() {
  Cleanup();
}

bool HEVCDecoderImpl::ApplyConfig() {
  if (!codec_ctx_) {
    return false;
  }

  // Threading options
  if (config_.threads > 0) {
    codec_ctx_->thread_count = config_.threads;
  }
  
  codec_ctx_->thread_type = config_.frame_threads ? 
      FF_THREAD_FRAME : FF_THREAD_SLICE;

  // Latency options
  if (config_.low_latency) {
    av_opt_set(codec_ctx_->priv_data, "flags", "low_delay", 0);
    codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
  }

  // Error handling options
  if (config_.enable_error_concealment) {
    codec_ctx_->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
    codec_ctx_->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;
  }
  
  codec_ctx_->err_recognition = config_.error_resilience;
  
  if (config_.skip_corrupted_frames) {
    codec_ctx_->flags2 |= AV_CODEC_FLAG2_SKIP_MANUAL;
  }

  // Performance options
  if (config_.fast_decode) {
    av_opt_set(codec_ctx_->priv_data, "preset", "ultrafast", 0);
    codec_ctx_->flags2 |= AV_CODEC_FLAG2_FAST;
  }
  
  if (config_.skip_loop_filter) {
    codec_ctx_->skip_loop_filter = AVDISCARD_ALL;
  }
  
  codec_ctx_->skip_frame = static_cast<AVDiscard>(config_.skip_frame);

  // Output format options
  if (config_.output_10bit) {
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P10LE;
  } else {
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
  }
  
  if (!config_.output_crop) {
    codec_ctx_->flags |= AV_CODEC_FLAG_UNALIGNED;
  }

  // Debug options
  if (config_.debug_mode) {
    codec_ctx_->debug = config_.debug_level;
  }

  // Reference frame management
  codec_ctx_->refs = config_.max_references;

  // Post-processing
  if (config_.enable_post_processing) {
    av_opt_set_int(codec_ctx_->priv_data, "postprocess", 1, 0);
    av_opt_set_int(codec_ctx_->priv_data, "quality", config_.post_processing_quality, 0);
  }

  // Apply bitstream filters if specified
  if (!config_.bitstream_filters.empty()) {
    av_opt_set(codec_ctx_->priv_data, "bsf", config_.bitstream_filters.c_str(), 0);
  }

  return true;
}

bool HEVCDecoderImpl::Initialize() {
  // Find HEVC decoder
  codec_ = avcodec_find_decoder(AV_CODEC_ID_HEVC);
  if (!codec_) {
    std::cerr << "HEVC codec not found" << std::endl;
    return false;
  }

  // Allocate codec context
  codec_ctx_ = avcodec_alloc_context3(codec_);
  if (!codec_ctx_) {
    std::cerr << "Failed to allocate codec context" << std::endl;
    return false;
  }

  // Apply configuration to codec context
  if (!ApplyConfig()) {
    std::cerr << "Failed to apply configuration" << std::endl;
    Cleanup();
    return false;
  }

  // Open the codec
  if (avcodec_open2(codec_ctx_, codec_, nullptr) < 0) {
    std::cerr << "Failed to open codec" << std::endl;
    Cleanup();
    return false;
  }

  // Allocate frame and packet
  av_frame_ = av_frame_alloc();
  if (!av_frame_) {
    std::cerr << "Failed to allocate frame" << std::endl;
    Cleanup();
    return false;
  }

  av_packet_ = av_packet_alloc();
  if (!av_packet_) {
    std::cerr << "Failed to allocate packet" << std::endl;
    Cleanup();
    return false;
  }

  initialized_ = true;
  return true;
}

int HEVCDecoderImpl::DecodeToYUV420(std::vector<uint8_t>* yuv_frame,
                                   const std::vector<uint8_t>* hevc_frame) {
  if (!initialized_ || !yuv_frame || !hevc_frame) {
    return 0;  // Error
  }

  // Fill packet with input data
  av_packet_unref(av_packet_);
  av_packet_->data = const_cast<uint8_t*>(hevc_frame->data());
  av_packet_->size = static_cast<int>(hevc_frame->size());

  // Send packet to decoder
  int send_result = avcodec_send_packet(codec_ctx_, av_packet_);
  if (send_result < 0) {
    std::cerr << "Error sending packet for decoding: " << send_result << std::endl;
    return 0;  // Error
  }

  // Receive frame
  int receive_result = avcodec_receive_frame(codec_ctx_, av_frame_);
  if (receive_result < 0) {
    if (receive_result != AVERROR(EAGAIN) && receive_result != AVERROR_EOF) {
      std::cerr << "Error during decoding: " << receive_result << std::endl;
    }
    return 0;  // Error or need more data
  }

  // Check frame format
  if (av_frame_->format != AV_PIX_FMT_YUV420P && 
      av_frame_->format != AV_PIX_FMT_YUV420P10LE) {
    std::cerr << "Unexpected pixel format: " << av_frame_->format << std::endl;
    return 0;  // Error
  }

  // Calculate required buffer size
  int y_size = av_frame_->linesize[0] * av_frame_->height;
  int u_size = av_frame_->linesize[1] * av_frame_->height / 2;
  int v_size = av_frame_->linesize[2] * av_frame_->height / 2;
  int total_size = y_size + u_size + v_size;

  // Resize output buffer
  yuv_frame->resize(total_size);

  // Copy Y plane
  std::memcpy(yuv_frame->data(), av_frame_->data[0], y_size);
  
  // Copy U plane
  std::memcpy(yuv_frame->data() + y_size, av_frame_->data[1], u_size);
  
  // Copy V plane
  std::memcpy(yuv_frame->data() + y_size + u_size, av_frame_->data[2], v_size);

  return 1;  // Success
}

int HEVCDecoderImpl::GetWidth() const {
  return initialized_ ? codec_ctx_->width : 0;
}

int HEVCDecoderImpl::GetHeight() const {
  return initialized_ ? codec_ctx_->height : 0;
}

void HEVCDecoderImpl::Flush() {
  if (initialized_) {
    avcodec_flush_buffers(codec_ctx_);
  }
}

void HEVCDecoderImpl::Reset() {
  Cleanup();
  initialized_ = Initialize();
}

void HEVCDecoderImpl::Cleanup() {
  if (av_packet_) {
    av_packet_unref(av_packet_);
    av_packet_free(&av_packet_);
    av_packet_ = nullptr;
  }

  if (av_frame_) {
    av_frame_unref(av_frame_);
    av_frame_free(&av_frame_);
    av_frame_ = nullptr;
  }

  if (codec_ctx_) {
    avcodec_free_context(&codec_ctx_);
    codec_ctx_ = nullptr;
  }

  initialized_ = false;
}

bool HEVCDecoderImpl::UpdateConfig(const HEVCDecoderConfig& config) {
  // Store new config
  config_ = config;
  
  // Some parameters can be updated without reinitializing the decoder
  if (initialized_ && codec_ctx_) {
    // Update thread count
    if (config_.threads > 0) {
      codec_ctx_->thread_count = config_.threads;
    }
    
    // Update skip frame
    codec_ctx_->skip_frame = static_cast<AVDiscard>(config_.skip_frame);
    
    // Update error resilience
    codec_ctx_->err_recognition = config_.error_resilience;
    
    // Update debug level
    if (config_.debug_mode) {
      codec_ctx_->debug = config_.debug_level;
    } else {
      codec_ctx_->debug = 0;
    }
    
    return true;
  }
  
  return false;
}

HEVCDecoderConfig HEVCDecoderImpl::GetConfig() const {
  return config_;
}

}  // namespace

std::unique_ptr<HEVCDecoder> HEVCDecoder::Create(const HEVCDecoderConfig& config) {
  // Register all codecs and formats (for older FFmpeg versions)
  #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
  #endif

  auto decoder = std::make_unique<HEVCDecoderImpl>(config);
  if (!decoder->Initialize()) {
    return nullptr;
  }
  
  return decoder;
}

}  // namespace media