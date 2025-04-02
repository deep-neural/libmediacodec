#include "hevc_encoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
}

#include <iostream>
#include <map>

namespace media {

namespace {

// Mapping from our enums to libav strings
const std::map<HEVCPreset, const char*> kPresetMap = {
    {HEVCPreset::ULTRAFAST, "ultrafast"},
    {HEVCPreset::SUPERFAST, "superfast"},
    {HEVCPreset::VERYFAST, "veryfast"},
    {HEVCPreset::FASTER, "faster"},
    {HEVCPreset::FAST, "fast"},
    {HEVCPreset::MEDIUM, "medium"},
    {HEVCPreset::SLOW, "slow"},
    {HEVCPreset::SLOWER, "slower"},
    {HEVCPreset::VERYSLOW, "veryslow"},
    {HEVCPreset::PLACEBO, "placebo"}
};

const std::map<HEVCProfile, const char*> kProfileMap = {
    {HEVCProfile::MAIN, "main"},
    {HEVCProfile::MAIN_10, "main10"},
    {HEVCProfile::MAIN_STILL_PICTURE, "mainstillpicture"},
    {HEVCProfile::REXT, "rext"}
};

const std::map<HEVCTier, const char*> kTierMap = {
    {HEVCTier::MAIN, "main"},
    {HEVCTier::HIGH, "high"}
};

const std::map<HEVCTune, const char*> kTuneMap = {
    {HEVCTune::NONE, ""},
    {HEVCTune::PSNR, "psnr"},
    {HEVCTune::SSIM, "ssim"},
    {HEVCTune::GRAIN, "grain"},
    {HEVCTune::ZEROLATENCY, "zerolatency"},
    {HEVCTune::FASTDECODE, "fastdecode"},
    {HEVCTune::ANIMATION, "animation"}
};

class HEVCEncoderImpl : public HEVCEncoder {
public:
    HEVCEncoderImpl() 
        : codec_(nullptr), codec_context_(nullptr), frame_(nullptr), packet_(nullptr),
          frames_encoded_(0), total_bytes_(0), total_bits_(0) {}

    ~HEVCEncoderImpl() override {
        if (codec_context_) {
            avcodec_free_context(&codec_context_);
        }
        if (frame_) {
            av_frame_free(&frame_);
        }
        if (packet_) {
            av_packet_free(&packet_);
        }
    }

    bool Initialize(const HEVCEncoderConfig& config) {
        config_ = config;
        
        // Find the HEVC encoder
        codec_ = avcodec_find_encoder(AV_CODEC_ID_HEVC);
        if (!codec_) {
            std::cerr << "Could not find HEVC encoder" << std::endl;
            return false;
        }

        // Allocate codec context
        codec_context_ = avcodec_alloc_context3(codec_);
        if (!codec_context_) {
            std::cerr << "Could not allocate video codec context" << std::endl;
            return false;
        }

        // Set basic codec parameters
        codec_context_->width = config.width;
        codec_context_->height = config.height;
        codec_context_->bit_rate = config.bitrate;
        codec_context_->time_base = AVRational{1, config.framerate};
        codec_context_->framerate = AVRational{config.framerate, 1};
        codec_context_->gop_size = config.keyint_max;
        codec_context_->max_b_frames = config.bframes;
        codec_context_->pix_fmt = AV_PIX_FMT_YUV420P;
        codec_context_->thread_count = config.threads;
        codec_context_->slices = config.slice_max_count;

        // Set codec options
        const char* preset_str = kPresetMap.at(config.preset);
        av_opt_set(codec_context_->priv_data, "preset", preset_str, 0);
        
        const char* profile_str = kProfileMap.at(config.profile);
        av_opt_set(codec_context_->priv_data, "profile", profile_str, 0);
        
        if (config.tier != HEVCTier::MAIN) {
            const char* tier_str = kTierMap.at(config.tier);
            av_opt_set(codec_context_->priv_data, "tier", tier_str, 0);
        }
        
        if (config.level > 0) {
            char level_str[10];
            snprintf(level_str, sizeof(level_str), "%.1f", config.level);
            av_opt_set(codec_context_->priv_data, "level", level_str, 0);
        }
        
        if (config.tune != HEVCTune::NONE) {
            const char* tune_str = kTuneMap.at(config.tune);
            av_opt_set(codec_context_->priv_data, "tune", tune_str, 0);
        }
        
        // Set rate control parameters
        switch (config.rc_mode) {
            case RateControlMode::CRF:
                av_opt_set_int(codec_context_->priv_data, "crf", config.crf, 0);
                break;
            case RateControlMode::CQP:
                av_opt_set_int(codec_context_->priv_data, "qp", config.qp, 0);
                break;
            case RateControlMode::ABR:
                codec_context_->bit_rate = config.bitrate;
                if (config.max_bitrate > 0) {
                    codec_context_->rc_max_rate = config.max_bitrate;
                }
                if (config.buffer_size > 0) {
                    codec_context_->rc_buffer_size = config.buffer_size;
                }
                break;
            case RateControlMode::CBR:
                codec_context_->bit_rate = config.bitrate;
                codec_context_->rc_max_rate = config.bitrate;
                codec_context_->rc_min_rate = config.bitrate;
                if (config.buffer_size > 0) {
                    codec_context_->rc_buffer_size = config.buffer_size;
                } else {
                    codec_context_->rc_buffer_size = config.bitrate;
                }
                break;
        }
        
        // VBV settings
        if (config.vbv_maxrate > 0) {
            codec_context_->rc_max_rate = config.vbv_maxrate;
        }
        if (config.vbv_bufsize > 0) {
            codec_context_->rc_buffer_size = config.vbv_bufsize;
        }
        
        // GOP structure settings
        if (config.keyint_min > 0) {
            av_opt_set_int(codec_context_->priv_data, "keyint_min", config.keyint_min, 0);
        }
        if (config.scenecut >= 0) {
            av_opt_set_int(codec_context_->priv_data, "scenecut", config.scenecut, 0);
        }
        av_opt_set_int(codec_context_->priv_data, "open-gop", config.open_gop ? 1 : 0, 0);
        av_opt_set_int(codec_context_->priv_data, "b-pyramid", config.b_pyramid ? 1 : 0, 0);
        
        // Quality settings
        av_opt_set_int(codec_context_->priv_data, "aq-mode", config.aq_mode ? 1 : 0, 0);
        if (config.aq_strength > 0) {
            av_opt_set_int(codec_context_->priv_data, "aq-strength", config.aq_strength, 0);
        }
        
        av_opt_set_int(codec_context_->priv_data, "psy", config.psy ? 1 : 0, 0);
        av_opt_set_int(codec_context_->priv_data, "psy-rd", config.psy_rd, 0);
        av_opt_set_int(codec_context_->priv_data, "psy-rdoq", config.psy_rdoq, 0);
        
        // Motion estimation settings
        if (config.me_range > 0) {
            av_opt_set_int(codec_context_->priv_data, "me_range", config.me_range, 0);
        }
        av_opt_set_int(codec_context_->priv_data, "subme", config.subme_level, 0);
        av_opt_set_int(codec_context_->priv_data, "me", config.me_method, 0);
        
        // Slice settings
        if (config.slice_max_size > 0) {
            av_opt_set_int(codec_context_->priv_data, "slice-max-size", config.slice_max_size, 0);
        }
        
        // Deblocking filter settings
        av_opt_set_int(codec_context_->priv_data, "deblock", config.deblock ? 1 : 0, 0);
        if (config.deblock && (config.deblock_alpha != 0 || config.deblock_beta != 0)) {
            char deblock_str[20];
            snprintf(deblock_str, sizeof(deblock_str), "%d:%d", 
                     config.deblock_alpha, config.deblock_beta);
            av_opt_set(codec_context_->priv_data, "deblock", deblock_str, 0);
        }
        
        // SAO settings
        av_opt_set_int(codec_context_->priv_data, "sao", config.sao ? 1 : 0, 0);
        
        // HEVC-specific settings
        av_opt_set_int(codec_context_->priv_data, "strong-intra-smoothing", 
                       config.strong_intra_smoothing ? 1 : 0, 0);
        av_opt_set_int(codec_context_->priv_data, "constrained-intra", 
                       config.constrained_intra ? 1 : 0, 0);
        av_opt_set_int(codec_context_->priv_data, "cu-lossless", 
                       config.cu_lossless ? 1 : 0, 0);
        av_opt_set_int(codec_context_->priv_data, "early-skip", 
                       config.early_skip ? 1 : 0, 0);
        
        // Output format
        av_opt_set_int(codec_context_->priv_data, "repeat-headers", 
                       config.repeat_headers ? 1 : 0, 0);
        av_opt_set_int(codec_context_->priv_data, "annexb", 
                       config.annexb ? 1 : 0, 0);
        
        // VUI parameters
        if (config.vui_parameters) {
            codec_context_->color_range = config.fullrange ? AVCOL_RANGE_JPEG : AVCOL_RANGE_MPEG;
        }

        // Open the codec
        int ret = avcodec_open2(codec_context_, codec_, nullptr);
        if (ret < 0) {
            char error_buffer[AV_ERROR_MAX_STRING_SIZE] = {0};
            av_strerror(ret, error_buffer, AV_ERROR_MAX_STRING_SIZE);
            std::cerr << "Could not open codec: " << error_buffer << std::endl;
            return false;
        }

        // Allocate frame
        frame_ = av_frame_alloc();
        if (!frame_) {
            std::cerr << "Could not allocate video frame" << std::endl;
            return false;
        }

        frame_->format = codec_context_->pix_fmt;
        frame_->width = codec_context_->width;
        frame_->height = codec_context_->height;

        ret = av_frame_get_buffer(frame_, 0);
        if (ret < 0) {
            std::cerr << "Could not allocate frame data" << std::endl;
            return false;
        }

        // Allocate packet
        packet_ = av_packet_alloc();
        if (!packet_) {
            std::cerr << "Could not allocate packet" << std::endl;
            return false;
        }

        frame_count_ = 0;
        frames_encoded_ = 0;
        total_bytes_ = 0;
        total_bits_ = 0;

        return true;
    }

    int EncodeYUV420(const std::vector<uint8_t>& yuv_data, 
                    std::vector<uint8_t>* encoded_frame) override {
        if (!codec_context_ || !frame_ || !packet_) {
            return 0;
        }

        // Make sure the frame is writable
        int ret = av_frame_make_writable(frame_);
        if (ret < 0) {
            std::cerr << "Could not make frame writable" << std::endl;
            return 0;
        }

        // Calculate plane sizes
        int y_size = codec_context_->width * codec_context_->height;
        int u_size = (codec_context_->width / 2) * (codec_context_->height / 2);
        int v_size = u_size;

        // Verify input size
        if (yuv_data.size() < y_size + u_size + v_size) {
            std::cerr << "Input data size is too small" << std::endl;
            return 0;
        }

        // Fill frame data
        std::memcpy(frame_->data[0], yuv_data.data(), y_size);
        std::memcpy(frame_->data[1], yuv_data.data() + y_size, u_size);
        std::memcpy(frame_->data[2], yuv_data.data() + y_size + u_size, v_size);

        frame_->pts = frame_count_++;

        // Encode the frame
        ret = avcodec_send_frame(codec_context_, frame_);
        if (ret < 0) {
            std::cerr << "Error sending frame for encoding" << std::endl;
            return 0;
        }

        return ReceivePacket(encoded_frame);
    }

    int Flush(std::vector<uint8_t>* encoded_frame) override {
        int ret = avcodec_send_frame(codec_context_, nullptr);
        if (ret < 0) {
            std::cerr << "Error flushing encoder" << std::endl;
            return 0;
        }

        return ReceivePacket(encoded_frame);
    }
    
    void GetStats(int* frames_encoded, double* avg_bitrate) const override {
        if (frames_encoded) {
            *frames_encoded = frames_encoded_;
        }
        if (avg_bitrate && frames_encoded_ > 0) {
            // Calculate average bitrate in bits per second
            double duration = static_cast<double>(frames_encoded_) / config_.framerate;
            *avg_bitrate = total_bits_ / duration;
        }
    }
    
    bool UpdateParams(int new_bitrate, int new_framerate) override {
        if (new_bitrate > 0) {
            av_opt_set_int(codec_context_->priv_data, "bitrate", new_bitrate, 0);
            config_.bitrate = new_bitrate;
        }
        
        if (new_framerate > 0) {
            codec_context_->time_base = AVRational{1, new_framerate};
            codec_context_->framerate = AVRational{new_framerate, 1};
            config_.framerate = new_framerate;
        }
        
        return true;
    }

private:
    int ReceivePacket(std::vector<uint8_t>* encoded_frame) {
        int ret = avcodec_receive_packet(codec_context_, packet_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // Need more input or end of stream
            return 1;
        } else if (ret < 0) {
            std::cerr << "Error receiving packet from encoder" << std::endl;
            return 0;
        }

        // Copy packet data to output vector
        encoded_frame->resize(packet_->size);
        std::memcpy(encoded_frame->data(), packet_->data, packet_->size);
        
        // Update stats
        frames_encoded_++;
        total_bytes_ += packet_->size;
        total_bits_ += packet_->size * 8;

        av_packet_unref(packet_);
        return 1;
    }

    const AVCodec* codec_;
    AVCodecContext* codec_context_;
    AVFrame* frame_;
    AVPacket* packet_;
    int64_t frame_count_;
    HEVCEncoderConfig config_;
    
    // Stats
    int frames_encoded_;
    int64_t total_bytes_;
    int64_t total_bits_;
};

}  // namespace

std::unique_ptr<HEVCEncoder> HEVCEncoder::Create(const HEVCEncoderConfig& config) {
    auto encoder = std::make_unique<HEVCEncoderImpl>();
    if (!encoder->Initialize(config)) {
        return nullptr;
    }
    return encoder;
}

}  // namespace media