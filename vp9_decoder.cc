#include "vp9_decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/frame.h>
#include <libavutil/error.h>
}

#include <cstring>
#include <iostream>
#include <fstream>

namespace media {

namespace {

// Helper function to convert FFmpeg error codes to string (avoiding av_err2str macro issues)
std::string error_to_string(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, errnum);
    return std::string(errbuf);
}

// Implementation of VP9Decoder that uses FFmpeg for decoding
class FFmpegVP9Decoder : public VP9Decoder {
 public:
  explicit FFmpegVP9Decoder(const VP9DecoderConfig& config)
      : config_(config),
        codec_(nullptr),
        codec_context_(nullptr),
        frame_(nullptr),
        parser_context_(nullptr),
        initialized_(false),
        width_(0),
        height_(0) {}

  ~FFmpegVP9Decoder() override {
    Cleanup();
  }

  // Initializes the decoder
  bool Initialize() {
    // Find the VP9 decoder
    codec_ = avcodec_find_decoder(AV_CODEC_ID_VP9);
    if (!codec_) {
      std::cerr << "VP9 codec not found!" << std::endl;
      return false;
    }

    // Create codec context
    codec_context_ = avcodec_alloc_context3(codec_);
    if (!codec_context_) {
      std::cerr << "Could not allocate codec context!" << std::endl;
      return false;
    }

    // Apply configuration parameters
    ApplyConfig();

    // Open the codec
    if (avcodec_open2(codec_context_, codec_, nullptr) < 0) {
      std::cerr << "Could not open codec!" << std::endl;
      return false;
    }

    // Allocate frame
    frame_ = av_frame_alloc();
    if (!frame_) {
      std::cerr << "Could not allocate frame!" << std::endl;
      return false;
    }

    // Initialize parser
    parser_context_ = av_parser_init(AV_CODEC_ID_VP9);
    if (!parser_context_) {
      std::cerr << "Could not initialize parser!" << std::endl;
      return false;
    }

    initialized_ = true;
    return true;
  }

  int DecodeToYUV420(const std::vector<uint8_t>& vp9_frame,
                    std::vector<uint8_t>* yuv_data) override {
    if (!initialized_ && !Initialize()) {
      return 0;
    }

    if (vp9_frame.empty() || !yuv_data) {
      return 0;
    }

    // Create packet
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
      std::cerr << "Could not allocate packet!" << std::endl;
      return 0;
    }

    // Set packet data
    packet->data = const_cast<uint8_t*>(vp9_frame.data());
    packet->size = static_cast<int>(vp9_frame.size());

    // Send packet to decoder
    int ret = avcodec_send_packet(codec_context_, packet);
    if (ret < 0) {
      std::cerr << "Error sending packet for decoding: " << error_to_string(ret) << std::endl;
      av_packet_free(&packet);
      return 0;
    }

    // Receive frame from decoder
    ret = avcodec_receive_frame(codec_context_, frame_);
    if (ret < 0) {
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        // Need more data or end of file
        av_packet_free(&packet);
        return 0;
      }
      std::cerr << "Error during decoding: " << error_to_string(ret) << std::endl;
      av_packet_free(&packet);
      return 0;
    }

    // Update width and height
    width_ = frame_->width;
    height_ = frame_->height;

    // Convert frame data to YUV420 format and store in yuv_data
    size_t y_size = frame_->width * frame_->height;
    size_t u_size = (frame_->width / 2) * (frame_->height / 2);
    size_t v_size = u_size;
    size_t total_size = y_size + u_size + v_size;

    yuv_data->resize(total_size);

    // Copy Y plane
    std::memcpy(yuv_data->data(), frame_->data[0], y_size);

    // Copy U plane
    std::memcpy(yuv_data->data() + y_size, frame_->data[1], u_size);

    // Copy V plane
    std::memcpy(yuv_data->data() + y_size + u_size, frame_->data[2], v_size);

    // Handle debug visualization if enabled
    if (config_.debug_visualization) {
      DumpFrameForDebug();
    }

    // Clean up
    av_packet_free(&packet);

    return 1;
  }

  int GetWidth() const override {
    return width_;
  }

  int GetHeight() const override {
    return height_;
  }

  void Reset() override {
    if (initialized_) {
      avcodec_flush_buffers(codec_context_);
    }
  }
  
  bool UpdateConfig(const VP9DecoderConfig& config) override {
    if (!initialized_) {
      config_ = config;
      return true;
    }
    
    // For parameters that can be updated mid-stream
    bool need_reopen = false;
    
    // Check if any parameters require reopening the codec
    need_reopen |= (config_.threads != config.threads);
    need_reopen |= (config_.frame_threading != config.frame_threading);
    need_reopen |= (config_.slice_threading != config.slice_threading);
    need_reopen |= (config_.low_delay != config.low_delay);
    
    config_ = config;
    
    if (need_reopen) {
      // Save current state
      int old_width = width_;
      int old_height = height_;
      
      // Close and reopen with new parameters
      Cleanup();
      bool success = Initialize();
      
      // Restore state
      width_ = old_width;
      height_ = old_height;
      
      return success;
    } else {
      // Apply immediate configuration changes
      ApplyConfig();
      return true;
    }
  }
  
  VP9DecoderConfig GetConfig() const override {
    return config_;
  }

 private:
  void Cleanup() {
    if (parser_context_) {
      av_parser_close(parser_context_);
      parser_context_ = nullptr;
    }

    if (frame_) {
      av_frame_free(&frame_);
      frame_ = nullptr;
    }

    if (codec_context_) {
      avcodec_free_context(&codec_context_);
      codec_context_ = nullptr;
    }

    initialized_ = false;
  }
  
  void ApplyConfig() {
    if (!codec_context_) {
      return;
    }
    
    // Threading configuration
    codec_context_->thread_count = config_.threads;
    if (config_.frame_threading) {
      codec_context_->thread_type |= FF_THREAD_FRAME;
    } else {
      codec_context_->thread_type &= ~FF_THREAD_FRAME;
    }
    if (config_.slice_threading) {
      codec_context_->thread_type |= FF_THREAD_SLICE;
    } else {
      codec_context_->thread_type &= ~FF_THREAD_SLICE;
    }
    
    // Performance/latency options
    if (config_.low_delay) {
      codec_context_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    } else {
      codec_context_->flags &= ~AV_CODEC_FLAG_LOW_DELAY;
    }
    
    codec_context_->skip_loop_filter = static_cast<AVDiscard>(config_.skip_loop_filter);
    codec_context_->skip_frame = static_cast<AVDiscard>(config_.skip_frame);
    
    // Error resilience/concealment
    if (config_.error_concealment) {
      // AV_CODEC_FLAG_ERROR_CONCEALMENT is deprecated/removed in newer FFmpeg
      // Apply related options via private data if available
      av_opt_set_int(codec_context_->priv_data, "enable_er", 1, 0);
    }
    
    codec_context_->err_recognition = config_.max_error_rate;
    
    // Memory management - using av_opt_set_int for tile-related options
    if (config_.max_tile_cols > 0) {
      av_opt_set_int(codec_context_->priv_data, "tile-columns", 
                    config_.max_tile_cols, 0);
    }
    
    if (config_.max_tile_rows > 0) {
      av_opt_set_int(codec_context_->priv_data, "tile-rows", 
                    config_.max_tile_rows, 0);
    }
    
    if (config_.max_threads_per_tile > 0) {
      av_opt_set_int(codec_context_->priv_data, "threads_per_tile", 
                    config_.max_threads_per_tile, 0);
    }
    
    // Set max dimensions through codec private options instead
    if (config_.max_width > 0) {
      av_opt_set_int(codec_context_->priv_data, "max_width", 
                     config_.max_width, 0);
    }
    
    if (config_.max_height > 0) {
      av_opt_set_int(codec_context_->priv_data, "max_height", 
                     config_.max_height, 0);
    }
    
    // Enable film grain if available in this FFmpeg version
    av_opt_set_int(codec_context_->priv_data, "apply-grain", 
                  config_.enable_film_grain ? 1 : 0, 0);
    
    // Post-processing options
    if (config_.enable_post_processing) {
      codec_context_->flags |= AV_CODEC_FLAG_LOOP_FILTER;
    } else {
      codec_context_->flags &= ~AV_CODEC_FLAG_LOOP_FILTER;
    }
    
    if (config_.deblocking_strength > 0) {
      av_opt_set_int(codec_context_->priv_data, "deblocklevel", 
                    config_.deblocking_strength, 0);
    }
    
    if (config_.deringing) {
      av_opt_set_int(codec_context_->priv_data, "deringing", 1, 0);
    }
    
    // Color conversion options
    if (config_.full_range) {
      codec_context_->color_range = AVCOL_RANGE_JPEG;
    } else {
      codec_context_->color_range = AVCOL_RANGE_MPEG;
    }
    
    if (config_.color_primaries > 0) {
      codec_context_->color_primaries = static_cast<AVColorPrimaries>(config_.color_primaries);
    }
    
    if (config_.color_trc > 0) {
      codec_context_->color_trc = static_cast<AVColorTransferCharacteristic>(config_.color_trc);
    }
    
    if (config_.colorspace > 0) {
      codec_context_->colorspace = static_cast<AVColorSpace>(config_.colorspace);
    }
    
    // Reference frame management
    if (config_.max_references > 0 && config_.max_references <= 8) {
      codec_context_->refs = config_.max_references;
    }
  }
  
  void DumpFrameForDebug() {
    if (config_.dump_frames_path.empty() || !frame_) {
      return;
    }
    
    static int frame_count = 0;
    std::string filename = config_.dump_frames_path + "/frame_" + 
                          std::to_string(frame_count++) + ".yuv";
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
      std::cerr << "Could not open debug output file: " << filename << std::endl;
      return;
    }
    
    // Write Y plane
    file.write(reinterpret_cast<const char*>(frame_->data[0]), 
              frame_->width * frame_->height);
    
    // Write U plane
    file.write(reinterpret_cast<const char*>(frame_->data[1]), 
              (frame_->width/2) * (frame_->height/2));
    
    // Write V plane
    file.write(reinterpret_cast<const char*>(frame_->data[2]), 
              (frame_->width/2) * (frame_->height/2));
    
    file.close();
    
    std::cout << "Dumped debug frame to " << filename << std::endl;
  }

  VP9DecoderConfig config_;
  const AVCodec* codec_;
  AVCodecContext* codec_context_;
  AVFrame* frame_;
  AVCodecParserContext* parser_context_;
  bool initialized_;
  int width_;
  int height_;
};

}  // namespace

std::unique_ptr<VP9Decoder> VP9Decoder::Create(const VP9DecoderConfig& config) {
  auto decoder = std::make_unique<FFmpegVP9Decoder>(config);
  if (!decoder->Initialize()) {
    return nullptr;
  }
  return decoder;
}

}  // namespace media