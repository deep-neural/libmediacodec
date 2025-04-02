// av1_decoder.cc
#include "av1_decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

#include <cstring>
#include <iostream>
#include <map>

namespace media {

namespace {

// Helper function to map string to AVColorPrimaries
AVColorPrimaries GetColorPrimaries(const std::string& primaries) {
  static const std::map<std::string, AVColorPrimaries> kColorPrimariesMap = {
      {"bt709", AVCOL_PRI_BT709},
      {"bt470m", AVCOL_PRI_BT470M},
      {"bt470bg", AVCOL_PRI_BT470BG},
      {"smpte170m", AVCOL_PRI_SMPTE170M},
      {"smpte240m", AVCOL_PRI_SMPTE240M},
      {"film", AVCOL_PRI_FILM},
      {"bt2020", AVCOL_PRI_BT2020},
      {"smpte428", AVCOL_PRI_SMPTE428},
      {"smpte431", AVCOL_PRI_SMPTE431},
      {"smpte432", AVCOL_PRI_SMPTE432},
      {"jedec-p22", AVCOL_PRI_JEDEC_P22},
  };
  
  auto it = kColorPrimariesMap.find(primaries);
  return it != kColorPrimariesMap.end() ? it->second : AVCOL_PRI_UNSPECIFIED;
}

// Helper function to map string to AVColorTransferCharacteristic
AVColorTransferCharacteristic GetColorTransferCharacteristic(const std::string& trc) {
  static const std::map<std::string, AVColorTransferCharacteristic> kColorTrcMap = {
      {"bt709", AVCOL_TRC_BT709},
      {"gamma22", AVCOL_TRC_GAMMA22},
      {"gamma28", AVCOL_TRC_GAMMA28},
      {"smpte170m", AVCOL_TRC_SMPTE170M},
      {"smpte240m", AVCOL_TRC_SMPTE240M},
      {"linear", AVCOL_TRC_LINEAR},
      {"log", AVCOL_TRC_LOG},
      {"log_sqrt", AVCOL_TRC_LOG_SQRT},
      {"iec61966_2_4", AVCOL_TRC_IEC61966_2_4},
      {"bt1361", AVCOL_TRC_BT1361_ECG},
      {"iec61966", AVCOL_TRC_IEC61966_2_1},
      {"bt2020_10bit", AVCOL_TRC_BT2020_10},
      {"bt2020_12bit", AVCOL_TRC_BT2020_12},
      {"smpte2084", AVCOL_TRC_SMPTE2084},
      {"pq", AVCOL_TRC_SMPTE2084},
      {"smpte428", AVCOL_TRC_SMPTE428},
      {"hlg", AVCOL_TRC_ARIB_STD_B67},
  };
  
  auto it = kColorTrcMap.find(trc);
  return it != kColorTrcMap.end() ? it->second : AVCOL_TRC_UNSPECIFIED;
}

// Helper function to map string to AVColorSpace
AVColorSpace GetColorSpace(const std::string& colorspace) {
  static const std::map<std::string, AVColorSpace> kColorSpaceMap = {
      {"rgb", AVCOL_SPC_RGB},
      {"bt709", AVCOL_SPC_BT709},
      {"fcc", AVCOL_SPC_FCC},
      {"bt470bg", AVCOL_SPC_BT470BG},
      {"smpte170m", AVCOL_SPC_SMPTE170M},
      {"smpte240m", AVCOL_SPC_SMPTE240M},
      {"ycgco", AVCOL_SPC_YCGCO},
      {"bt2020nc", AVCOL_SPC_BT2020_NCL},
      {"bt2020c", AVCOL_SPC_BT2020_CL},
      {"smpte2085", AVCOL_SPC_SMPTE2085},
      {"chroma_derived_nc", AVCOL_SPC_CHROMA_DERIVED_NCL},
      {"chroma_derived_c", AVCOL_SPC_CHROMA_DERIVED_CL},
      {"ictcp", AVCOL_SPC_ICTCP},
  };
  
  auto it = kColorSpaceMap.find(colorspace);
  return it != kColorSpaceMap.end() ? it->second : AVCOL_SPC_UNSPECIFIED;
}

// Helper function to map string to AVColorRange
AVColorRange GetColorRange(const std::string& range) {
  if (range == "tv" || range == "limited") {
    return AVCOL_RANGE_MPEG;
  } else if (range == "pc" || range == "full") {
    return AVCOL_RANGE_JPEG;
  }
  return AVCOL_RANGE_UNSPECIFIED;
}

class AV1DecoderImpl : public AV1Decoder {
 public:
  explicit AV1DecoderImpl(const AV1DecoderConfig& config) : config_(config) {}

  ~AV1DecoderImpl() override {
    if (codec_ctx_) {
      avcodec_free_context(&codec_ctx_);
    }
    if (frame_) {
      av_frame_free(&frame_);
    }
    if (parser_ctx_) {
      av_parser_close(parser_ctx_);
    }
    if (packet_) {
      av_packet_free(&packet_);
    }
  }

  bool Initialize() {
    // Find the AV1 decoder
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_AV1);
    if (!codec) {
      std::cerr << "AV1 codec not found" << std::endl;
      return false;
    }

    // Create codec context
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
      std::cerr << "Failed to allocate codec context" << std::endl;
      return false;
    }

    // Apply basic configuration
    ApplyBasicConfig();
    
    // Apply performance settings
    ApplyPerformanceSettings();
    
    // Apply visual quality settings
    ApplyVisualQualitySettings();
    
    // Apply error resilience settings
    ApplyErrorResilienceSettings();
    
    // Apply decoder implementation details
    ApplyDecoderImplementationDetails();
    
    // Apply post-processing settings
    ApplyPostProcessingSettings();
    
    // Apply debug settings
    ApplyDebugSettings();
    
    // Apply memory management settings
    ApplyMemoryManagementSettings();
    
    // Apply color conversion settings
    ApplyColorConversionSettings();

    // Open the codec
    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
      std::cerr << "Failed to open codec" << std::endl;
      return false;
    }

    // Create parser context
    parser_ctx_ = av_parser_init(AV_CODEC_ID_AV1);
    if (!parser_ctx_) {
      std::cerr << "Failed to initialize parser" << std::endl;
      return false;
    }

    // Allocate frame
    frame_ = av_frame_alloc();
    if (!frame_) {
      std::cerr << "Failed to allocate frame" << std::endl;
      return false;
    }

    // Allocate packet
    packet_ = av_packet_alloc();
    if (!packet_) {
      std::cerr << "Failed to allocate packet" << std::endl;
      return false;
    }

    initialized_ = true;
    return true;
  }

  int DecodeToYUV420(std::vector<uint8_t>& yuv_frame,
                     const std::vector<uint8_t>* av1_frame) override {
    if (!initialized_ && !Initialize()) {
      return 0;
    }

    if (!av1_frame || av1_frame->empty()) {
      std::cerr << "Invalid input frame" << std::endl;
      return 0;
    }

    // Reset packet
    av_packet_unref(packet_);
    
    // Parse the input data
    const uint8_t* data = av1_frame->data();
    int data_size = static_cast<int>(av1_frame->size());
    uint8_t* parsed_data = nullptr;
    int parsed_size = 0;
    
    parsed_size = av_parser_parse2(parser_ctx_, codec_ctx_, &parsed_data, &parsed_size,
                                 data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
    
    if (parsed_size < 0) {
      std::cerr << "Error during parsing" << std::endl;
      return 0;
    }
    
    // Set up the packet
    packet_->data = parsed_data;
    packet_->size = parsed_size;

    // Send packet to decoder
    int ret = avcodec_send_packet(codec_ctx_, packet_);
    if (ret < 0) {
      std::cerr << "Error sending packet for decoding" << std::endl;
      return 0;
    }

    // Receive frame
    ret = avcodec_receive_frame(codec_ctx_, frame_);
    if (ret < 0) {
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        // Need more data
        return 0;
      }
      std::cerr << "Error during decoding" << std::endl;
      return 0;
    }

    // Get frame dimensions
    width_ = frame_->width;
    height_ = frame_->height;

    // Calculate required buffer size for YUV420 format
    int y_size = width_ * height_;
    int u_size = (width_ / 2) * (height_ / 2);
    int v_size = u_size;
    int total_size = y_size + u_size + v_size;

    // Resize output buffer
    yuv_frame.resize(total_size);

    // Copy Y plane
    std::memcpy(yuv_frame.data(), frame_->data[0], y_size);
    
    // Copy U plane
    std::memcpy(yuv_frame.data() + y_size, frame_->data[1], u_size);
    
    // Copy V plane
    std::memcpy(yuv_frame.data() + y_size + u_size, frame_->data[2], v_size);

    // Unref frame for next decode
    av_frame_unref(frame_);

    return 1;
  }

  void Reset() override {
    if (codec_ctx_) {
      avcodec_flush_buffers(codec_ctx_);
    }
  }

  int GetWidth() const override {
    return width_;
  }

  int GetHeight() const override {
    return height_;
  }

 private:
  void ApplyBasicConfig() {
    // Thread management
    codec_ctx_->thread_count = config_.threads;
    if (config_.frame_parallel) {
      codec_ctx_->thread_type = FF_THREAD_FRAME;
    } else {
      codec_ctx_->thread_type = FF_THREAD_SLICE;
    }
    
    // Low delay mode
    if (config_.low_delay) {
      codec_ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    }
  }
  
  void ApplyPerformanceSettings() {
    // Skip frames
    if (config_.skip_frames > 0) {
      codec_ctx_->skip_frame = static_cast<AVDiscard>(config_.skip_frames);
    }
    
    // Skip loop filter
    if (config_.skip_loop_filter > 0) {
      codec_ctx_->skip_loop_filter = static_cast<AVDiscard>(config_.skip_loop_filter);
    }
    
    // Skip IDCT
    if (config_.skip_idct > 0) {
      codec_ctx_->skip_idct = static_cast<AVDiscard>(config_.skip_idct);
    }
  }
  
  void ApplyVisualQualitySettings() {
    // Film grain application
    av_opt_set_int(codec_ctx_->priv_data, "apply-grain", config_.enable_film_grain ? 1 : 0, 0);
    
    // Annex-B format
    av_opt_set_int(codec_ctx_->priv_data, "annexb", config_.enable_annex_b ? 1 : 0, 0);
    
    // CDEF filter
    av_opt_set_int(codec_ctx_->priv_data, "enable-cdef", config_.enable_cdef ? 1 : 0, 0);
    
    // Loop restoration filter
    av_opt_set_int(codec_ctx_->priv_data, "enable-restoration", config_.enable_restoration ? 1 : 0, 0);
    
    // Warped motion
    av_opt_set_int(codec_ctx_->priv_data, "enable-warped-motion", config_.enable_warped_motion ? 1 : 0, 0);
    
    // Reference frame motion vectors
    av_opt_set_int(codec_ctx_->priv_data, "enable-ref-frame-mvs", config_.enable_ref_frame_mvs ? 1 : 0, 0);
    
    // Intra block copy
    av_opt_set_int(codec_ctx_->priv_data, "enable-intrabc", config_.enable_intrabc ? 1 : 0, 0);
    
    // Masked compound prediction
    av_opt_set_int(codec_ctx_->priv_data, "enable-masked-comp", config_.enable_masked_comp ? 1 : 0, 0);
    
    // Dual filter
    av_opt_set_int(codec_ctx_->priv_data, "enable-dual-filter", config_.enable_dual_filter ? 1 : 0, 0);
    
    // Order hint
    av_opt_set_int(codec_ctx_->priv_data, "enable-order-hint", config_.enable_order_hint ? 1 : 0, 0);
    
    // Joint compound prediction
    av_opt_set_int(codec_ctx_->priv_data, "enable-jnt-comp", config_.enable_jnt_comp ? 1 : 0, 0);
  }
  
  void ApplyErrorResilienceSettings() {
    // Error resilience
    if (config_.error_resilient) {
      codec_ctx_->err_recognition = AV_EF_CAREFUL;
      codec_ctx_->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
    }
    
    // Strict standard compliance
    if (config_.strict_std_compliance) {
      codec_ctx_->strict_std_compliance = FF_COMPLIANCE_STRICT;
    } else {
      codec_ctx_->strict_std_compliance = FF_COMPLIANCE_NORMAL;
    }
  }
  
  void ApplyDecoderImplementationDetails() {
    // Operating point
    if (config_.operating_point >= 0 && config_.operating_point <= 31) {
      av_opt_set_int(codec_ctx_->priv_data, "operating-point", config_.operating_point, 0);
    }
    
    // Tile threads
    av_opt_set_int(codec_ctx_->priv_data, "tile-threads", config_.tile_threads, 0);
    
    // Row-based multi-threading
    av_opt_set_int(codec_ctx_->priv_data, "row-mt", config_.row_mt ? 1 : 0, 0);
    
    // Uncompressed header
    av_opt_set_int(codec_ctx_->priv_data, "enable-uncompressed-header", config_.enable_uncompressed_header ? 1 : 0, 0);
  }
  
  void ApplyPostProcessingSettings() {
    // Post-processing
    codec_ctx_->flags |= config_.enable_postproc ? AV_CODEC_FLAG_LOOP_FILTER : 0;
    
    // Post-processing strength
    if (config_.enable_postproc && config_.postproc_strength > 0) {
      av_opt_set_int(codec_ctx_->priv_data, "postproc", config_.postproc_strength, 0);
    }
  }
  
  void ApplyDebugSettings() {
    // Debug all
    if (config_.debug_all) {
      codec_ctx_->debug = FF_DEBUG_PICT_INFO | FF_DEBUG_MB_TYPE | FF_DEBUG_QP;
    }
    
    // Print info
    if (config_.print_info) {
      av_opt_set_int(codec_ctx_->priv_data, "print-info", 1, 0);
    }
  }
  
  void ApplyMemoryManagementSettings() {
    // Max threads
    if (config_.max_threads > 0) {
      codec_ctx_->thread_count = std::min(config_.threads, config_.max_threads);
    }
    
    // Max frame threads
    if (config_.max_frame_threads > 0) {
      av_opt_set_int(codec_ctx_->priv_data, "frame-threads", config_.max_frame_threads, 0);
    }
    
    // Max tile threads
    if (config_.max_tile_threads > 0) {
      av_opt_set_int(codec_ctx_->priv_data, "tile-threads", std::min(config_.tile_threads, config_.max_tile_threads), 0);
    }
    
    // Max memory
    if (config_.max_memory > 0) {
      av_opt_set_int(codec_ctx_->priv_data, "max-memory", config_.max_memory, 0);
    }
  }
  
  void ApplyColorConversionSettings() {
    // Color primaries
    if (!config_.color_primaries.empty()) {
      codec_ctx_->color_primaries = GetColorPrimaries(config_.color_primaries);
    }
    
    // Transfer characteristics
    if (!config_.color_trc.empty()) {
      codec_ctx_->color_trc = GetColorTransferCharacteristic(config_.color_trc);
    }
    
    // Colorspace
    if (!config_.colorspace.empty()) {
      codec_ctx_->colorspace = GetColorSpace(config_.colorspace);
    }
    
    // Color range
    if (!config_.color_range.empty()) {
      codec_ctx_->color_range = GetColorRange(config_.color_range);
    }
  }

  AV1DecoderConfig config_;
  AVCodecContext* codec_ctx_ = nullptr;
  AVCodecParserContext* parser_ctx_ = nullptr;
  AVFrame* frame_ = nullptr;
  AVPacket* packet_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  bool initialized_ = false;
};

}  // namespace

std::unique_ptr<AV1Decoder> AV1Decoder::Create(const AV1DecoderConfig& config) {
  auto decoder = std::make_unique<AV1DecoderImpl>(config);
  if (!decoder->Initialize()) {
    return nullptr;
  }
  return decoder;
}

}  // namespace media