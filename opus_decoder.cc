// opus_decoder.cc
#include "opus_decoder.h"

#include <memory>
#include <vector>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace media {

namespace {

// Implementation of the OPUSDecoder interface
class OPUSDecoderImpl : public OPUSDecoder {
 public:
  OPUSDecoderImpl(const OPUSDecoderConfig& config)
      : config_(config),
        codec_(nullptr),
        codec_context_(nullptr),
        parser_(nullptr),
        frame_(nullptr),
        packet_(nullptr),
        swr_context_(nullptr),
        last_error_("") {}

  ~OPUSDecoderImpl() override {
    CleanUp();
  }

  static std::unique_ptr<OPUSDecoderImpl> Create(const OPUSDecoderConfig& config) {
    auto decoder = std::make_unique<OPUSDecoderImpl>(config);
    if (!decoder->Initialize()) {
      return nullptr;
    }
    return decoder;
  }

  int DecodeToPCM_S16LE(const std::vector<uint8_t>& opus_frame,
                       std::vector<uint8_t>* pcm_frame) override {
    return DecodeAndConvertToPCM(opus_frame, pcm_frame, AV_SAMPLE_FMT_S16, false);
  }

  int DecodeToPCM_U8(const std::vector<uint8_t>& opus_frame,
                    std::vector<uint8_t>* pcm_frame) override {
    return DecodeAndConvertToPCM(opus_frame, pcm_frame, AV_SAMPLE_FMT_U8, false);
  }

  int DecodeToPCM_F32BE(const std::vector<uint8_t>& opus_frame,
                       std::vector<uint8_t>* pcm_frame) override {
    return DecodeAndConvertToPCM(opus_frame, pcm_frame, AV_SAMPLE_FMT_FLT, true);
  }
  
  bool UpdateConfig(const OPUSDecoderConfig& config) override {
    // Store the new configuration
    config_ = config;
    
    // Close and clean up existing codec context
    if (codec_context_) {
      avcodec_free_context(&codec_context_);
    }
    
    // Recreate and initialize with new settings
    bool result = Initialize();
    if (!result) {
      last_error_ = "Failed to reinitialize decoder with new configuration";
    }
    
    return result;
  }
  
  const char* GetLastError() const override {
    return last_error_.c_str();
  }
  
  void Reset() override {
    if (codec_context_) {
      avcodec_flush_buffers(codec_context_);
    }
  }

 private:
  bool Initialize() {
    // Find the OPUS decoder
    codec_ = avcodec_find_decoder(AV_CODEC_ID_OPUS);
    if (!codec_) {
      last_error_ = "Could not find OPUS decoder";
      return false;
    }

    // Create a codec context
    codec_context_ = avcodec_alloc_context3(codec_);
    if (!codec_context_) {
      last_error_ = "Could not allocate codec context";
      return false;
    }

    // Set codec parameters
    codec_context_->sample_rate = config_.sample_rate;
    
    // Set up channel layout based on FFmpeg version
    #if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
    // New API: use ch_layout structure
    av_channel_layout_default(&codec_context_->ch_layout, config_.channels);
    #else
    // Old API: use channels and channel_layout fields
    codec_context_->channels = config_.channels;
    codec_context_->channel_layout = config_.channels == 1 ? 
        AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    #endif
    
    // Set additional OPUS-specific options
    
    // Apply gain (in dB)
    av_opt_set_int(codec_context_->priv_data, "apply_phase_inv", 1, 0);
    av_opt_set_int(codec_context_->priv_data, "gain", config_.gain_db, 0);
    
    // Forward Error Correction
    av_opt_set_int(codec_context_->priv_data, "fec", config_.use_fec ? 1 : 0, 0);
    
    // Discontinuous Transmission
    av_opt_set_int(codec_context_->priv_data, "dtx", config_.use_dtx ? 1 : 0, 0);
    
    // Packet loss percentage
    av_opt_set_int(codec_context_->priv_data, "packet_loss", config_.packet_loss_percentage, 0);
    
    // Low latency mode
    if (config_.low_latency_mode) {
      codec_context_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    }
    
    // Constrained VBR mode
    av_opt_set_int(codec_context_->priv_data, "vbr", config_.constrained_vbr ? 2 : 1, 0);
    
    // Set maximum bandwidth
    const char* bandwidth = "fullband"; // Default
    switch (config_.max_bandwidth) {
      case OPUSDecoderConfig::Bandwidth::NARROWBAND:
        bandwidth = "narrowband";
        break;
      case OPUSDecoderConfig::Bandwidth::MEDIUMBAND:
        bandwidth = "mediumband";
        break;
      case OPUSDecoderConfig::Bandwidth::WIDEBAND:
        bandwidth = "wideband";
        break;
      case OPUSDecoderConfig::Bandwidth::SUPERWIDEBAND:
        bandwidth = "superwideband";
        break;
      case OPUSDecoderConfig::Bandwidth::FULLBAND:
        bandwidth = "fullband";
        break;
    }
    av_opt_set(codec_context_->priv_data, "bandwidth", bandwidth, 0);
    
    // Frame size (convert ms to samples)
    int frame_size = static_cast<int>(config_.frame_size_ms * config_.sample_rate / 1000);
    av_opt_set_int(codec_context_->priv_data, "frame_size", frame_size, 0);
    
    // PLC buffer size
    av_opt_set_int(codec_context_->priv_data, "plc_buffer", config_.plc_buffer_size, 0);

    // Open the codec
    int result = avcodec_open2(codec_context_, codec_, nullptr);
    if (result < 0) {
      char error_buf[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(result, error_buf, AV_ERROR_MAX_STRING_SIZE);
      last_error_ = "Failed to open codec: ";
      last_error_ += error_buf;
      return false;
    }

    // Create the parser
    parser_ = av_parser_init(AV_CODEC_ID_OPUS);
    if (!parser_) {
      last_error_ = "Failed to initialize parser";
      return false;
    }

    // Allocate frame and packet
    frame_ = av_frame_alloc();
    if (!frame_) {
      last_error_ = "Failed to allocate frame";
      return false;
    }

    packet_ = av_packet_alloc();
    if (!packet_) {
      last_error_ = "Failed to allocate packet";
      return false;
    }

    return true;
  }

  void CleanUp() {
    if (swr_context_) {
      swr_free(&swr_context_);
      swr_context_ = nullptr;
    }
    if (packet_) {
      av_packet_free(&packet_);
      packet_ = nullptr;
    }
    if (frame_) {
      av_frame_free(&frame_);
      frame_ = nullptr;
    }
    if (parser_) {
      av_parser_close(parser_);
      parser_ = nullptr;
    }
    if (codec_context_) {
      avcodec_free_context(&codec_context_);
      codec_context_ = nullptr;
    }
  }

  int DecodeAndConvertToPCM(const std::vector<uint8_t>& opus_frame,
                           std::vector<uint8_t>* pcm_frame,
                           AVSampleFormat target_format,
                           bool big_endian) {
    if (!pcm_frame) {
      last_error_ = "Null output buffer provided";
      return 0;
    }
    
    if (opus_frame.empty()) {
      last_error_ = "Empty input frame";
      return 0;
    }

    // Reset the packet and set data
    av_packet_unref(packet_);
    packet_->data = const_cast<uint8_t*>(opus_frame.data());
    packet_->size = static_cast<int>(opus_frame.size());

    // Send packet to decoder
    int result = avcodec_send_packet(codec_context_, packet_);
    if (result < 0) {
      char error_buf[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(result, error_buf, AV_ERROR_MAX_STRING_SIZE);
      last_error_ = "Failed to send packet to decoder: ";
      last_error_ += error_buf;
      return 0;
    }

    // Receive frame from decoder
    result = avcodec_receive_frame(codec_context_, frame_);
    if (result < 0) {
      if (result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
        char error_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(result, error_buf, AV_ERROR_MAX_STRING_SIZE);
        last_error_ = "Failed to receive frame from decoder: ";
        last_error_ += error_buf;
      } else {
        last_error_ = "Need more data to decode";
      }
      return 0;
    }

    // Prepare for resampling if needed
    if (!PrepareResamplingContext(target_format, big_endian)) {
      return 0;
    }

    // Determine output buffer size and allocate
    int dst_samples = av_rescale_rnd(
        swr_get_delay(swr_context_, frame_->sample_rate) + frame_->nb_samples,
        config_.sample_rate,
        frame_->sample_rate,
        AV_ROUND_UP);

    int bytes_per_sample = av_get_bytes_per_sample(target_format);
    int buffer_size = dst_samples * config_.channels * bytes_per_sample;
    pcm_frame->resize(buffer_size);

    // Convert audio samples
    uint8_t* output_buffer = pcm_frame->data();
    result = swr_convert(
        swr_context_,
        &output_buffer,
        dst_samples,
        (const uint8_t**)frame_->data,
        frame_->nb_samples);

    if (result < 0) {
      char error_buf[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(result, error_buf, AV_ERROR_MAX_STRING_SIZE);
      last_error_ = "Failed to convert audio samples: ";
      last_error_ += error_buf;
      pcm_frame->clear();
      return 0;
    }

    // Adjust the size of the output buffer to the actual converted size
    int actual_buffer_size = result * config_.channels * bytes_per_sample;
    pcm_frame->resize(actual_buffer_size);
    
    // Handle big-endian conversion for float if needed
    if (target_format == AV_SAMPLE_FMT_FLT && big_endian) {
      SwapEndianness(*pcm_frame, bytes_per_sample);
    }

    return 1;
  }

  bool PrepareResamplingContext(AVSampleFormat target_format, bool big_endian) {
    // Free existing context if it exists
    if (swr_context_) {
      swr_free(&swr_context_);
    }

    // Create a new resampling context
    swr_context_ = swr_alloc();
    if (!swr_context_) {
      last_error_ = "Failed to allocate resampling context";
      return false;
    }

    // Set input parameters based on FFmpeg version
    #if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
    // New API: use ch_layout structure
    av_opt_set_chlayout(swr_context_, "in_chlayout", &frame_->ch_layout, 0);
    #else
    // Old API: use channel_layout field
    av_opt_set_channel_layout(swr_context_, "in_channel_layout", frame_->channel_layout, 0);
    #endif
    
    av_opt_set_int(swr_context_, "in_sample_rate", frame_->sample_rate, 0);
    av_opt_set_sample_fmt(swr_context_, "in_sample_fmt", static_cast<AVSampleFormat>(frame_->format), 0);

    // Set output parameters based on FFmpeg version
    #if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
    // New API: use ch_layout structure
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, config_.channels);
    av_opt_set_chlayout(swr_context_, "out_chlayout", &out_ch_layout, 0);
    #else
    // Old API: use channel_layout field
    uint64_t out_channel_layout = config_.channels == 1 ? 
        AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
    av_opt_set_channel_layout(swr_context_, "out_channel_layout", out_channel_layout, 0);
    #endif
    
    av_opt_set_int(swr_context_, "out_sample_rate", config_.sample_rate, 0);
    av_opt_set_sample_fmt(swr_context_, "out_sample_fmt", target_format, 0);

    // Initialize the resampler
    if (swr_init(swr_context_) < 0) {
      last_error_ = "Failed to initialize audio resampler";
      swr_free(&swr_context_);
      swr_context_ = nullptr;
      return false;
    }

    return true;
  }
  
  // Utility function to swap endianness for F32BE output format
  void SwapEndianness(std::vector<uint8_t>& data, int bytes_per_sample) {
    if (bytes_per_sample != 4) {
      return;  // Only handle 4-byte floats
    }
    
    for (size_t i = 0; i < data.size(); i += bytes_per_sample) {
      std::swap(data[i], data[i + 3]);
      std::swap(data[i + 1], data[i + 2]);
    }
  }

  OPUSDecoderConfig config_;
  const AVCodec* codec_;
  AVCodecContext* codec_context_;
  AVCodecParserContext* parser_;
  AVFrame* frame_;
  AVPacket* packet_;
  SwrContext* swr_context_;
  std::string last_error_;
};

}  // namespace

std::unique_ptr<OPUSDecoder> OPUSDecoder::Create(const OPUSDecoderConfig& config) {
  return OPUSDecoderImpl::Create(config);
}

}  // namespace media