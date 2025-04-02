#include "vp8_encoder.h"

#include <iostream>
#include <fstream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

namespace media {

VP8Encoder* VP8Encoder::Create(const VP8EncoderConfig& config) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_VP8);
    if (!codec) {
        return nullptr;
    }

    auto encoder = new VP8Encoder(config);
    if (!encoder || !encoder->ApplyCodecOptions(config)) {
        delete encoder;
        return nullptr;
    }

    return encoder;
}

VP8Encoder::VP8Encoder(const VP8EncoderConfig& config)
    : initialized_(false),
      first_pass_complete_(false),
      config_(config),
      codec_context_(nullptr) {
}

VP8Encoder::~VP8Encoder() {
    if (codec_context_) {
        avcodec_free_context(&codec_context_);
    }
}

bool VP8Encoder::ApplyCodecOptions(const VP8EncoderConfig& config) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_VP8);
    if (!codec) {
        return false;
    }

    codec_context_ = avcodec_alloc_context3(codec);
    if (!codec_context_) {
        return false;
    }

    // Basic parameters
    codec_context_->width = config.width;
    codec_context_->height = config.height;
    codec_context_->bit_rate = config.bitrate;
    codec_context_->time_base = AVRational{1, config.framerate};
    codec_context_->framerate = AVRational{config.framerate, 1};
    codec_context_->pix_fmt = AV_PIX_FMT_YUV420P;
    
    // Threading
    if (config.thread_count > 0) {
        codec_context_->thread_count = config.thread_count;
    }

    // Two-pass encoding setup
    if (config.two_pass_encoding) {
        if (!first_pass_complete_) {
            // First pass
            av_opt_set(codec_context_->priv_data, "pass", "1", 0);
            av_opt_set(codec_context_->priv_data, "stats", config.stats_file.c_str(), 0);
        } else {
            // Second pass
            av_opt_set(codec_context_->priv_data, "pass", "2", 0);
            av_opt_set(codec_context_->priv_data, "stats", config.stats_file.c_str(), 0);
        }
    }

    // Set all VP8-specific options
    // Quality and bitrate control
    if (config.quality >= 0 && config.quality <= 63) {
        av_opt_set_int(codec_context_->priv_data, "crf", config.quality, 0);
    }
    
    av_opt_set_int(codec_context_->priv_data, "qmin", config.min_quantizer, 0);
    av_opt_set_int(codec_context_->priv_data, "qmax", config.max_quantizer, 0);
    
    if (config.buffer_size > 0) {
        codec_context_->rc_buffer_size = config.buffer_size;
    }
    
    codec_context_->rc_initial_buffer_occupancy = 
        static_cast<int>(config.buffer_initial_size * codec_context_->rc_buffer_size);
    
    // Rate control mode
    switch (config.rc_mode) {
        case VP8EncoderConfig::RC_MODE_CBR:
            av_opt_set(codec_context_->priv_data, "rc_mode", "CBR", 0);
            break;
        case VP8EncoderConfig::RC_MODE_VBR:
            av_opt_set(codec_context_->priv_data, "rc_mode", "VBR", 0);
            break;
        case VP8EncoderConfig::RC_MODE_CQ:
            av_opt_set(codec_context_->priv_data, "rc_mode", "CQ", 0);
            break;
    }
    
    // Keyframe settings
    codec_context_->gop_size = config.keyframe_interval;
    if (config.keyframe_min_interval > 0) {
        av_opt_set_int(codec_context_->priv_data, "keyint_min", config.keyframe_min_interval, 0);
    }
    
    // Deadline/speed control
    const char* deadline_value = "good";
    switch (config.deadline) {
        case VP8EncoderConfig::DEADLINE_BEST_QUALITY:
            deadline_value = "best";
            break;
        case VP8EncoderConfig::DEADLINE_GOOD_QUALITY:
            deadline_value = "good";
            break;
        case VP8EncoderConfig::DEADLINE_REALTIME:
            deadline_value = "realtime";
            break;
    }
    av_opt_set(codec_context_->priv_data, "deadline", deadline_value, 0);
    
    // CPU usage
    av_opt_set_int(codec_context_->priv_data, "cpu-used", config.cpu_used, 0);
    
    // Error resilience
    av_opt_set_int(codec_context_->priv_data, "error_resilient", config.error_resilient ? 1 : 0, 0);
    
    // Noise sensitivity
    av_opt_set_int(codec_context_->priv_data, "noise_sensitivity", config.noise_sensitivity, 0);
    
    // Sharpness
    av_opt_set_int(codec_context_->priv_data, "sharpness", config.sharpness, 0);
    
    // Static threshold
    av_opt_set_int(codec_context_->priv_data, "static_thresh", config.static_threshold, 0);
    
    // Token partitions
    av_opt_set_int(codec_context_->priv_data, "token_partitions", config.token_partitions, 0);
    
    // Arnr settings
    if (config.arnr_enabled) {
        av_opt_set_int(codec_context_->priv_data, "arnr_max_frames", config.arnr_max_frames, 0);
        av_opt_set_int(codec_context_->priv_data, "arnr_strength", config.arnr_strength, 0);
        av_opt_set_int(codec_context_->priv_data, "arnr_type", config.arnr_type, 0);
    }
    
    // Lag in frames for lookahead
    if (config.lag_in_frames > 0) {
        av_opt_set_int(codec_context_->priv_data, "lag-in-frames", config.lag_in_frames, 0);
    }
    
    // Initialize the codec context
    if (avcodec_open2(codec_context_, codec, nullptr) < 0) {
        avcodec_free_context(&codec_context_);
        return false;
    }
    
    initialized_ = true;
    return true;
}

int VP8Encoder::EncodeYUV420(const std::vector<uint8_t>& yuv_data, std::vector<uint8_t>* encoded_frame) {
    if (!initialized_ || !codec_context_) {
        return 0;
    }
    
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) return 0;

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        av_packet_free(&pkt);
        return 0;
    }

    frame->format = codec_context_->pix_fmt;
    frame->width = codec_context_->width;
    frame->height = codec_context_->height;

    // Allocate frame buffers
    if (av_frame_get_buffer(frame, 0) < 0) {
        av_frame_free(&frame);
        av_packet_free(&pkt);
        return 0;
    }
    
    // Make sure frame is writable
    if (av_frame_make_writable(frame) < 0) {
        av_frame_free(&frame);
        av_packet_free(&pkt);
        return 0;
    }

    // Calculate plane sizes
    int y_size = codec_context_->width * codec_context_->height;
    int uv_size = y_size / 4;
    
    // Make sure we have enough data
    if (yuv_data.size() < y_size + 2 * uv_size) {
        av_frame_free(&frame);
        av_packet_free(&pkt);
        return 0;
    }
    
    // Copy data to frame planes - Y plane
    std::copy(yuv_data.begin(), yuv_data.begin() + y_size, frame->data[0]);
    
    // U plane
    std::copy(yuv_data.begin() + y_size, yuv_data.begin() + y_size + uv_size, frame->data[1]);
    
    // V plane
    std::copy(yuv_data.begin() + y_size + uv_size, yuv_data.begin() + y_size + 2 * uv_size, frame->data[2]);

    int ret = avcodec_send_frame(codec_context_, frame);
    if (ret < 0) {
        av_frame_free(&frame);
        av_packet_free(&pkt);
        return 0;
    }

    ret = avcodec_receive_packet(codec_context_, pkt);
    if (ret == 0) {
        encoded_frame->resize(pkt->size);
        std::copy(pkt->data, pkt->data + pkt->size, encoded_frame->begin());
        av_packet_free(&pkt);
        av_frame_free(&frame);
        return 1;
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);
    return 0;
}

bool VP8Encoder::StartFirstPass() {
    if (config_.two_pass_encoding && !first_pass_complete_) {
        // Reset state if needed
        if (codec_context_) {
            avcodec_free_context(&codec_context_);
            initialized_ = false;
        }
        
        return ApplyCodecOptions(config_);
    }
    return false;
}

bool VP8Encoder::StartSecondPass() {
    if (config_.two_pass_encoding && !initialized_) {
        first_pass_complete_ = true;
        return ApplyCodecOptions(config_);
    }
    return false;
}

bool VP8Encoder::IsFirstPassComplete() const {
    return first_pass_complete_;
}

} // namespace media