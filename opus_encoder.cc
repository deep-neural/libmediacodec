#include "opus_encoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}

#include <string>
#include <iostream>
#include <cstring>

namespace media {

namespace {

// Helper function to convert from enum to FFmpeg int values
int ConvertApplicationType(OPUSApplication app) {
    return static_cast<int>(app);
}

int ConvertBandwidth(OPUSEncoderConfig::Bandwidth bandwidth) {
    return static_cast<int>(bandwidth);
}

int ConvertSignalType(OPUSEncoderConfig::SignalType signal_type) {
    return static_cast<int>(signal_type);
}

int ConvertPredictionDisabled(OPUSEncoderConfig::PredictionDisabled pred_disabled) {
    return static_cast<int>(pred_disabled);
}

class OPUSEncoderImpl : public OPUSEncoder {
public:
    explicit OPUSEncoderImpl(const OPUSEncoderConfig& config)
        : config_(config),
          codec_(nullptr),
          context_(nullptr),
          frame_(nullptr),
          packet_(nullptr),
          swr_ctx_(nullptr) {}

    ~OPUSEncoderImpl() override {
        Cleanup();
    }

    bool Initialize() {
        // Find the OPUS encoder
        codec_ = avcodec_find_encoder(AV_CODEC_ID_OPUS);
        if (!codec_) {
            last_error_ = "Codec OPUS not found";
            return false;
        }

        // Allocate codec context
        context_ = avcodec_alloc_context3(codec_);
        if (!context_) {
            last_error_ = "Could not allocate audio codec context";
            return false;
        }

        // Set encoding parameters
        context_->sample_fmt = AV_SAMPLE_FMT_FLT;  // OPUS uses float samples
        context_->sample_rate = config_.sample_rate;
        
        // Set channel layout based on the FFmpeg version
        // In newer FFmpeg versions, we need to use ch_layout instead of channels
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
        av_channel_layout_default(&context_->ch_layout, config_.channels);
#else
        context_->channels = config_.channels;
        context_->channel_layout = av_get_default_channel_layout(config_.channels);
#endif

        context_->bit_rate = config_.bitrate;
        context_->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;  // Allow experimental stuff

        // Set all OPUS-specific options
        // Application type
        av_opt_set_int(context_, "opus_application", ConvertApplicationType(config_.application), 0);
        
        // Complexity
        av_opt_set_int(context_, "complexity", config_.complexity, 0);
        
        // Forward Error Correction
        av_opt_set_int(context_, "fec", config_.use_inband_fec ? 1 : 0, 0);
        
        // Discontinuous Transmission
        av_opt_set_int(context_, "dtx", config_.use_dtx ? 1 : 0, 0);
        
        // Variable Bit Rate
        av_opt_set_int(context_, "vbr", config_.use_vbr ? 1 : 0, 0);
        
        // Constrained VBR
        av_opt_set_int(context_, "vbr_constraint", config_.use_cvbr ? 1 : 0, 0);
        
        // Bandwidth setting
        av_opt_set_int(context_, "bandwidth", ConvertBandwidth(config_.bandwidth), 0);
        
        // Packet Loss Percentage
        av_opt_set_int(context_, "packet_loss", config_.packet_loss_percentage, 0);
        
        // Signal type hint
        av_opt_set_int(context_, "signal", ConvertSignalType(config_.signal_type), 0);
        
        // LSB depth for bit-exact encoding
        av_opt_set_int(context_, "lsb_depth", config_.lsb_depth, 0);
        
        // Prediction control
        av_opt_set_int(context_, "prediction_disabled", ConvertPredictionDisabled(config_.prediction_disabled), 0);
        
        // Frame size limits - not directly supported in FFmpeg's OPUS encoder
        // but we can use them internally for our frame handling
        
        // Open the codec
        int ret = avcodec_open2(context_, codec_, nullptr);
        if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
            last_error_ = "Could not open codec: " + std::string(error_buf);
            return false;
        }

        // Calculate frame size based on sample rate and frame duration
        frame_size_ = (config_.sample_rate * config_.frame_duration_ms) / 1000;

        // Allocate frame and packet
        frame_ = av_frame_alloc();
        if (!frame_) {
            last_error_ = "Could not allocate audio frame";
            return false;
        }

        frame_->format = context_->sample_fmt;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
        frame_->ch_layout = context_->ch_layout;
#else
        frame_->channel_layout = context_->channel_layout;
        frame_->channels = context_->channels;
#endif
        frame_->sample_rate = context_->sample_rate;
        frame_->nb_samples = frame_size_;

        ret = av_frame_get_buffer(frame_, 0);
        if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
            last_error_ = "Could not allocate audio data buffers: " + std::string(error_buf);
            return false;
        }

        packet_ = av_packet_alloc();
        if (!packet_) {
            last_error_ = "Could not allocate packet";
            return false;
        }

        return true;
    }

    int EncodePCM_S16LE(const std::vector<uint8_t>& pcm_data, std::vector<uint8_t>* frame) override {
        return EncodeInternal(pcm_data, frame, AV_SAMPLE_FMT_S16, true);
    }

    int EncodePCM_U8(const std::vector<uint8_t>& pcm_data, std::vector<uint8_t>* frame) override {
        return EncodeInternal(pcm_data, frame, AV_SAMPLE_FMT_U8, true);
    }

    int EncodePCM_F32BE(const std::vector<uint8_t>& pcm_data, std::vector<uint8_t>* frame) override {
        return EncodeInternal(pcm_data, frame, AV_SAMPLE_FMT_FLT, false);
    }

    std::string GetLastError() const override {
        return last_error_;
    }

private:
    int EncodeInternal(const std::vector<uint8_t>& pcm_data, std::vector<uint8_t>* frame, 
                       AVSampleFormat input_format, bool is_input_packed) {
        if (!context_ || !codec_ || !frame_ || !packet_) {
            last_error_ = "Encoder not properly initialized";
            return 0;
        }

        if (frame == nullptr) {
            last_error_ = "Output frame buffer is null";
            return 0;
        }

        // Initialize resampler if needed for this format
        if (!InitializeResampler(input_format, is_input_packed)) {
            return 0;  // Error already set in InitializeResampler
        }

        // Make sure the frame is writable
        int ret = av_frame_make_writable(frame_);
        if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
            last_error_ = "Could not make frame writable: " + std::string(error_buf);
            return 0;
        }

        // Calculate number of samples in input
        size_t bytes_per_sample = GetBytesPerSample(input_format);
        if (bytes_per_sample == 0) {
            last_error_ = "Invalid sample format";
            return 0;
        }

        // Get the number of channels from the config
        int num_channels = config_.channels;
        
        int samples_in_input = pcm_data.size() / (bytes_per_sample * num_channels);
        
        // Ensure we have enough data for a full frame
        if (samples_in_input < frame_size_) {
            last_error_ = "Not enough input data for a full frame";
            return 0;
        }

        // Convert input to the format needed by the encoder
        if (swr_ctx_) {
            const uint8_t* in_data[AV_NUM_DATA_POINTERS] = {nullptr};
            
            // Set up input data pointers
            if (is_input_packed) {
                in_data[0] = pcm_data.data();
            } else {
                // For planar formats (unlikely for the requested formats)
                // This would require splitting the data into separate planes
                last_error_ = "Planar input format not supported in this implementation";
                return 0;
            }

            // Perform resampling/conversion
            ret = swr_convert(swr_ctx_, 
                              frame_->data, frame_size_,
                              in_data, frame_size_);
            
            if (ret < 0) {
                char error_buf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
                last_error_ = "Error during sample format conversion: " + std::string(error_buf);
                return 0;
            }
        } else {
            // Direct copy if no conversion is needed (unlikely)
            size_t data_size = frame_size_ * num_channels * GetBytesPerSample(context_->sample_fmt);
            if (pcm_data.size() < data_size) {
                last_error_ = "Input data size too small";
                return 0;
            }
            memcpy(frame_->data[0], pcm_data.data(), data_size);
        }

        // Set presentation timestamp
        frame_->pts = pts_;
        pts_ += frame_size_;

        // Send the frame to the encoder
        ret = avcodec_send_frame(context_, frame_);
        if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
            last_error_ = "Error sending frame to encoder: " + std::string(error_buf);
            return 0;
        }

        // Get encoded packet
        ret = avcodec_receive_packet(context_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // Need more frames or end of stream
            last_error_ = "Encoder needs more frames";
            return 0;
        } else if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
            last_error_ = "Error encoding audio frame: " + std::string(error_buf);
            return 0;
        }

        // Copy encoded data to output buffer
        frame->resize(packet_->size);
        memcpy(frame->data(), packet_->data, packet_->size);

        // Reset packet for reuse
        av_packet_unref(packet_);

        return 1;
    }

    bool InitializeResampler(AVSampleFormat input_format, bool is_input_packed) {
        // Don't reinitialize if already set up for this format
        if (swr_ctx_ && last_input_format_ == input_format) {
            return true;
        }

        // Cleanup existing resampler if any
        if (swr_ctx_) {
            swr_free(&swr_ctx_);
            swr_ctx_ = nullptr;
        }

        // Create a new resampler
        swr_ctx_ = swr_alloc();
        if (!swr_ctx_) {
            last_error_ = "Could not allocate resampler context";
            return false;
        }

        // Set input parameters
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
        AVChannelLayout in_ch_layout;
        av_channel_layout_default(&in_ch_layout, config_.channels);
        av_opt_set_chlayout(swr_ctx_, "in_chlayout", &in_ch_layout, 0);
#else
        av_opt_set_int(swr_ctx_, "in_channel_layout", av_get_default_channel_layout(config_.channels), 0);
        av_opt_set_int(swr_ctx_, "in_channels", config_.channels, 0);
#endif
        av_opt_set_int(swr_ctx_, "in_sample_rate", config_.sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt", input_format, 0);

        // Set output parameters
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 24, 100)
        AVChannelLayout out_ch_layout;
        av_channel_layout_default(&out_ch_layout, config_.channels);
        av_opt_set_chlayout(swr_ctx_, "out_chlayout", &out_ch_layout, 0);
#else
        av_opt_set_int(swr_ctx_, "out_channel_layout", av_get_default_channel_layout(config_.channels), 0);
        av_opt_set_int(swr_ctx_, "out_channels", config_.channels, 0);
#endif
        av_opt_set_int(swr_ctx_, "out_sample_rate", config_.sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", context_->sample_fmt, 0);

        // Initialize the resampler
        int ret = swr_init(swr_ctx_);
        if (ret < 0) {
            char error_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, error_buf, AV_ERROR_MAX_STRING_SIZE);
            last_error_ = "Failed to initialize the resampler: " + std::string(error_buf);
            swr_free(&swr_ctx_);
            swr_ctx_ = nullptr;
            return false;
        }

        last_input_format_ = input_format;
        return true;
    }

    size_t GetBytesPerSample(AVSampleFormat format) {
        switch (format) {
            case AV_SAMPLE_FMT_U8:
            case AV_SAMPLE_FMT_U8P:
                return 1;
            case AV_SAMPLE_FMT_S16:
            case AV_SAMPLE_FMT_S16P:
                return 2;
            case AV_SAMPLE_FMT_S32:
            case AV_SAMPLE_FMT_S32P:
            case AV_SAMPLE_FMT_FLT:
            case AV_SAMPLE_FMT_FLTP:
                return 4;
            case AV_SAMPLE_FMT_DBL:
            case AV_SAMPLE_FMT_DBLP:
                return 8;
            default:
                return 0;  // Unknown format
        }
    }

    void Cleanup() {
        if (swr_ctx_) {
            swr_free(&swr_ctx_);
            swr_ctx_ = nullptr;
        }
        
        if (packet_) {
            av_packet_free(&packet_);
            packet_ = nullptr;
        }
        
        if (frame_) {
            av_frame_free(&frame_);
            frame_ = nullptr;
        }
        
        if (context_) {
            avcodec_free_context(&context_);
            context_ = nullptr;
        }
    }

    OPUSEncoderConfig config_;
    const AVCodec* codec_;
    AVCodecContext* context_;
    AVFrame* frame_;
    AVPacket* packet_;
    SwrContext* swr_ctx_;
    AVSampleFormat last_input_format_ = AV_SAMPLE_FMT_NONE;
    std::string last_error_;
    int frame_size_ = 960;  // Default for 48kHz and 20ms
    int64_t pts_ = 0;
};

}  // namespace

std::unique_ptr<OPUSEncoder> OPUSEncoder::Create(const OPUSEncoderConfig& config) {
    auto encoder = std::make_unique<OPUSEncoderImpl>(config);
    
    if (!encoder->Initialize()) {
        return nullptr;
    }
    
    return encoder;
}

}  // namespace media