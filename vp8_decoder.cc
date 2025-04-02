#include "vp8_decoder.h"
#include <iostream>

VP8Decoder::VP8Decoder()
    : codec_context_(nullptr), frame_(nullptr), packet_(nullptr) {}

VP8Decoder::~VP8Decoder() {
    if (frame_) av_frame_free(&frame_);
    if (packet_) av_packet_free(&packet_);
    if (codec_context_) avcodec_free_context(&codec_context_);
}

std::shared_ptr<VP8Decoder> VP8Decoder::Create(const VP8DecoderConfig& config) {
    auto decoder = std::shared_ptr<VP8Decoder>(new VP8Decoder());
    if (!decoder->Initialize(config)) {
        return nullptr;
    }
    return decoder;
}

bool VP8Decoder::Initialize(const VP8DecoderConfig& config) {
    config_ = config;
    
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_VP8);
    if (!codec) {
        std::cerr << "Failed to find VP8 codec" << std::endl;
        return false;
    }

    codec_context_ = avcodec_alloc_context3(codec);
    if (!codec_context_) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return false;
    }

    // Set basic parameters
    if (config.width > 0) codec_context_->width = config.width;
    if (config.height > 0) codec_context_->height = config.height;
    
    // Set up threading
    if (config.thread_count > 0) {
        codec_context_->thread_count = config.thread_count;
    }
    codec_context_->thread_type = config.thread_type;
    
    // Error concealment and handling
    codec_context_->error_concealment = config.error_concealment;
    codec_context_->skip_loop_filter = static_cast<AVDiscard>(config.skip_loop_filter);
    codec_context_->skip_idct = static_cast<AVDiscard>(config.skip_idct);
    codec_context_->skip_frame = static_cast<AVDiscard>(config.skip_frame);
    
    // Low-level decoder settings
    codec_context_->flags = config.flags;
    codec_context_->flags2 = config.flags2;
    
    // Set output format
    codec_context_->pix_fmt = static_cast<AVPixelFormat>(config.pixel_format);
    
    // Decoder mode
    codec_context_->flags |= config.low_delay ? AV_CODEC_FLAG_LOW_DELAY : 0;
    
    // Debugging
    codec_context_->debug = config.debug;
    
    // Algorithm-specific options
    codec_context_->lowres = config.lowres;
    
    // Frame rate
    if (config.framerate.num > 0 && config.framerate.den > 0) {
        codec_context_->framerate = config.framerate;
    }
    
    // Timebase
    if (config.timebase.num > 0 && config.timebase.den > 0) {
        codec_context_->time_base = config.timebase;
    }
    
    // Error resilience
    codec_context_->err_recognition = config.err_recognition;
    
    // Set extradata if provided
    if (!config.extradata.empty()) {
        codec_context_->extradata_size = config.extradata.size();
        codec_context_->extradata = static_cast<uint8_t*>(av_mallocz(config.extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (!codec_context_->extradata) {
            std::cerr << "Failed to allocate extradata" << std::endl;
            return false;
        }
        memcpy(codec_context_->extradata, config.extradata.data(), config.extradata.size());
    }
    
    // Apply any tune settings
    if (!config.tune.empty()) {
        av_opt_set(codec_context_->priv_data, "tune", config.tune.c_str(), 0);
    }
    
    // Handle alpha channel
    if (config.output_alpha) {
        av_opt_set_int(codec_context_->priv_data, "alpha_quality", 100, 0);
    }

    if (avcodec_open2(codec_context_, codec, nullptr) < 0) {
        std::cerr << "Failed to open codec" << std::endl;
        return false;
    }

    frame_ = av_frame_alloc();
    if (!frame_) {
        std::cerr << "Failed to allocate frame" << std::endl;
        return false;
    }

    packet_ = av_packet_alloc();
    if (!packet_) {
        std::cerr << "Failed to allocate packet" << std::endl;
        return false;
    }

    return true;
}

int VP8Decoder::DecodeToYUV420(const std::vector<uint8_t>& vp8_frame, std::vector<uint8_t>* yuv_data) {
    av_packet_unref(packet_);
    packet_->data = const_cast<uint8_t*>(vp8_frame.data());
    packet_->size = vp8_frame.size();

    if (avcodec_send_packet(codec_context_, packet_) < 0) {
        std::cerr << "Failed to send packet" << std::endl;
        return false;
    }

    while (avcodec_receive_frame(codec_context_, frame_) >= 0) {
        int width = frame_->width;
        int height = frame_->height;
        int y_size = width * height;
        int uv_size = y_size / 4;

        yuv_data->resize(y_size + uv_size * 2);
        uint8_t* yuv_ptr = yuv_data->data();

        for (int i = 0; i < height; ++i)
            memcpy(yuv_ptr + i * width, frame_->data[0] + i * frame_->linesize[0], width);

        for (int i = 0; i < height / 2; ++i) {
            memcpy(yuv_ptr + y_size + i * (width / 2), frame_->data[1] + i * frame_->linesize[1], width / 2);
            memcpy(yuv_ptr + y_size + uv_size + i * (width / 2), frame_->data[2] + i * frame_->linesize[2], width / 2);
        }
    }

    return true;
}