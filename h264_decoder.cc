#include "h264_decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <algorithm>
#include <cstring>

namespace media {

namespace {

// Implementation of the H264Decoder interface using FFmpeg
class H264DecoderInstance : public H264Decoder {
 public:
  explicit H264DecoderInstance(const H264DecoderConfig& config)
      : config_(config),
        codec_(nullptr),
        codec_context_(nullptr),
        frame_(nullptr),
        packet_(nullptr),
        initialized_(false),
        frame_width_(0),
        frame_height_(0) {}

  ~H264DecoderInstance() override {
    if (frame_) {
      av_frame_free(&frame_);
    }
    if (packet_) {
      av_packet_free(&packet_);
    }
    if (codec_context_) {
      avcodec_free_context(&codec_context_);
    }
  }

  bool Initialize() {
    // Find the decoder
    codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec_) {
      return false;
    }

    // Allocate codec context
    codec_context_ = avcodec_alloc_context3(codec_);
    if (!codec_context_) {
      return false;
    }

    // Configure the decoder with basic parameters
    if (config_.width > 0 && config_.height > 0) {
      codec_context_->width = config_.width;
      codec_context_->height = config_.height;
    }
    
    // Apply all configuration options
    ApplyDecoderOptions();
    
    // Add extradata (SPS/PPS) if provided
    if (!config_.extradata.empty()) {
      codec_context_->extradata_size = static_cast<int>(config_.extradata.size());
      codec_context_->extradata = static_cast<uint8_t*>(
          av_mallocz(codec_context_->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
      if (codec_context_->extradata) {
        std::memcpy(codec_context_->extradata, config_.extradata.data(), 
                   codec_context_->extradata_size);
      }
    }

    // Open the codec
    int ret = avcodec_open2(codec_context_, codec_, nullptr);
    if (ret < 0) {
      return false;
    }

    // Allocate frame and packet
    frame_ = av_frame_alloc();
    if (!frame_) {
      return false;
    }

    packet_ = av_packet_alloc();
    if (!packet_) {
      return false;
    }

    initialized_ = true;
    return true;
  }

  bool IsInitialized() const override {
    return initialized_;
  }

  void Reset() override {
    if (codec_context_) {
      avcodec_flush_buffers(codec_context_);
      
      // Skip frames after flush if configured
      for (int i = 0; i < config_.skip_frames_after_flush; ++i) {
        std::vector<uint8_t> dummy;
        DecodeToYUV420(dummy, nullptr);
      }
    }
  }

  int DecodeToYUV420(std::vector<uint8_t>& yuv_frame, 
                     const std::vector<uint8_t>* h264_frame) override {
    if (!initialized_) {
      return -1;
    }

    int ret = 0;
    
    // Set up the packet with input data
    if (h264_frame && !h264_frame->empty()) {
      packet_->data = const_cast<uint8_t*>(h264_frame->data());
      packet_->size = static_cast<int>(h264_frame->size());
    } else {
      // Flush mode - send null packet to get any remaining frames
      packet_->data = nullptr;
      packet_->size = 0;
    }

    // Send packet to decoder
    ret = avcodec_send_packet(codec_context_, packet_);
    if (ret < 0) {
      // Error handling
      return ret;
    }

    // Receive decoded frame
    ret = avcodec_receive_frame(codec_context_, frame_);
    if (ret < 0) {
      if (ret == AVERROR(EAGAIN)) {
        // Need more data
        return 0;
      } else if (ret == AVERROR_EOF) {
        // End of stream
        return 0;
      } else {
        // Other error
        return ret;
      }
    }

    // Frame successfully decoded
    frame_width_ = frame_->width;
    frame_height_ = frame_->height;

    // Calculate required buffer size for YUV420 format
    int yuv_size = frame_->width * frame_->height * 3 / 2; // Y + U + V
    yuv_frame.resize(yuv_size);

    // Copy Y plane
    int y_size = frame_->width * frame_->height;
    std::memcpy(yuv_frame.data(), frame_->data[0], y_size);
    
    // Copy U plane
    int uv_stride = frame_->width / 2;
    int uv_height = frame_->height / 2;
    int u_size = uv_stride * uv_height;
    
    for (int i = 0; i < uv_height; i++) {
      std::memcpy(yuv_frame.data() + y_size + i * uv_stride,
                 frame_->data[1] + i * frame_->linesize[1],
                 uv_stride);
    }
    
    // Copy V plane
    for (int i = 0; i < uv_height; i++) {
      std::memcpy(yuv_frame.data() + y_size + u_size + i * uv_stride,
                 frame_->data[2] + i * frame_->linesize[2],
                 uv_stride);
    }

    return 1; // Success
  }

  void GetFrameDimensions(int* width, int* height) const override {
    if (width) {
      *width = frame_width_;
    }
    if (height) {
      *height = frame_height_;
    }
  }

 private:
  void ApplyDecoderOptions() {
    if (config_.thread_count > 0) {
      codec_context_->thread_count = config_.thread_count;
    }
    
    if (config_.low_delay) {
      codec_context_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    }
    
    if (config_.skip_loop_filter) {
      codec_context_->skip_loop_filter = AVDISCARD_ALL;
    }
    
    if (config_.skip_frame) {
      codec_context_->skip_frame = AVDISCARD_NONREF;
    }
    
    if (config_.error_concealment) {
      codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    }
    
    if (config_.skip_idct) {
      codec_context_->skip_idct = AVDISCARD_NONREF;
    }
    
    if (config_.error_recognition > 0) {
      codec_context_->err_recognition = config_.error_recognition;
    }
    
    if (config_.delay > 0) {
      codec_context_->delay = config_.delay;
    }
    
    if (config_.max_refs > 0) {
      av_opt_set_int(codec_context_->priv_data, "max_refs", config_.max_refs, 0);
    }
    
    // Configure threading mode
    if (config_.slice_threads) {
      av_opt_set_int(codec_context_->priv_data, "slice_threads", 1, 0);
    }
    
    if (!config_.frame_threads) {
      av_opt_set_int(codec_context_->priv_data, "frame_threads", 0, 0);
    }
    
    if (config_.qp_min > 0) {
      codec_context_->qmin = config_.qp_min;
    }
    
    if (config_.qp_max > 0) {
      codec_context_->qmax = config_.qp_max;
    }
    
    if (config_.max_b_frames > 0) {
      codec_context_->max_b_frames = config_.max_b_frames;
    }
    
    if (config_.pixel_format >= 0) {
      codec_context_->pix_fmt = static_cast<AVPixelFormat>(config_.pixel_format);
    }
    
    if (config_.refs > 0) {
      codec_context_->refs = config_.refs;
    }
    
    if (config_.profile >= 0) {
      codec_context_->profile = config_.profile;
    }
    
    if (config_.level >= 0) {
      codec_context_->level = config_.level;
    }
    
    if (!config_.output_in_display_order) {
      codec_context_->flags |= AV_CODEC_FLAG_OUTPUT_CORRUPT;
    }
    
    if (config_.strict_std_compliance) {
      codec_context_->strict_std_compliance = FF_COMPLIANCE_STRICT;
    }
    
    if (config_.log_level != -8) {
      av_log_set_level(config_.log_level);
    }
  }

  H264DecoderConfig config_;
  const AVCodec* codec_;
  AVCodecContext* codec_context_;
  AVFrame* frame_;
  AVPacket* packet_;
  bool initialized_;
  int frame_width_;
  int frame_height_;
};

}  // namespace

std::unique_ptr<H264Decoder> H264Decoder::Create(const H264DecoderConfig& config) {
  // Initialize FFmpeg globally if needed (in a thread-safe way)
  static bool ffmpeg_initialized = false;
  static std::mutex init_mutex;
  
  {
    std::lock_guard<std::mutex> lock(init_mutex);
    if (!ffmpeg_initialized) {
      // Register all codecs and formats
      #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
        av_register_all();
      #endif
      ffmpeg_initialized = true;
    }
  }
  
  auto decoder = std::make_unique<H264DecoderInstance>(config);
  if (!decoder->Initialize()) {
    return nullptr;
  }
  
  return decoder;
}

}  // namespace media