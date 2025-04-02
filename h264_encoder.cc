#include "h264_encoder.h"


#include <iostream>
#include <string>

namespace media {

namespace {

class H264EncoderInstance : public H264Encoder {
public:
    explicit H264EncoderInstance(const H264EncoderConfig& config)
        : config_(config), initialized_(false) {}
    
    ~H264EncoderInstance() override {
        Cleanup();
    }
    
    bool Initialize() {
        // Cleanup previous state if any
        Cleanup();
        
        // Find the H.264 encoder
        codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec_) {
            std::cerr << "Error: Could not find H.264 encoder" << std::endl;
            return false;
        }
        
        // Create encoder context
        codec_ctx_ = avcodec_alloc_context3(codec_);
        if (!codec_ctx_) {
            std::cerr << "Error: Could not allocate encoder context" << std::endl;
            return false;
        }
        
        // Set basic encoder parameters
        codec_ctx_->width = config_.width;
        codec_ctx_->height = config_.height;
        codec_ctx_->bit_rate = config_.bitrate;
        codec_ctx_->time_base = AVRational{1, config_.framerate};
        codec_ctx_->framerate = AVRational{config_.framerate, 1};
        codec_ctx_->gop_size = config_.gop_size;
        codec_ctx_->max_b_frames = config_.max_b_frames;
        codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
        codec_ctx_->refs = config_.refs;
        codec_ctx_->thread_count = config_.threads;
        codec_ctx_->slices = config_.slices;
        codec_ctx_->keyint_min = config_.keyint_min;
        codec_ctx_->trellis = config_.trellis;
        
        // Set codec-specific options
        av_opt_set(codec_ctx_->priv_data, "preset", config_.preset.c_str(), 0);
        av_opt_set(codec_ctx_->priv_data, "profile", config_.profile.c_str(), 0);
        
        // Set level if specified
        if (!config_.level.empty()) {
            av_opt_set(codec_ctx_->priv_data, "level", config_.level.c_str(), 0);
        }
        
        // Set tune if specified
        if (!config_.tune.empty()) {
            av_opt_set(codec_ctx_->priv_data, "tune", config_.tune.c_str(), 0);
        }
        
        // Rate control
        if (config_.constant_bitrate) {
            av_opt_set(codec_ctx_->priv_data, "crf", "0", 0);
            av_opt_set(codec_ctx_->priv_data, "tune", "zerolatency", 0);
        } else if (config_.qp >= 0) {
            char qp_str[8];
            snprintf(qp_str, sizeof(qp_str), "%d", config_.qp);
            av_opt_set(codec_ctx_->priv_data, "qp", qp_str, 0);
        } else {
            char crf_str[8];
            snprintf(crf_str, sizeof(crf_str), "%d", config_.crf);
            av_opt_set(codec_ctx_->priv_data, "crf", crf_str, 0);
        }
        
        // VBV settings
        if (config_.vbv_maxrate > 0) {
            codec_ctx_->rc_max_rate = config_.vbv_maxrate;
        }
        
        if (config_.vbv_bufsize > 0) {
            codec_ctx_->rc_buffer_size = config_.vbv_bufsize;
        }
        
        // Set x264 specific options via private data
        if (config_.rc_lookahead > 0) {
            char lookahead_str[8];
            snprintf(lookahead_str, sizeof(lookahead_str), "%d", config_.rc_lookahead);
            av_opt_set(codec_ctx_->priv_data, "rc-lookahead", lookahead_str, 0);
        }
        
        // Motion estimation
        av_opt_set(codec_ctx_->priv_data, "me_method", config_.me_method.c_str(), 0);
        
        char me_range_str[8];
        snprintf(me_range_str, sizeof(me_range_str), "%d", config_.me_range);
        av_opt_set(codec_ctx_->priv_data, "me_range", me_range_str, 0);
        
        char subpixel_str[8];
        snprintf(subpixel_str, sizeof(subpixel_str), "%d", config_.subpixel_me);
        av_opt_set(codec_ctx_->priv_data, "subq", subpixel_str, 0);
        
        // Analysis options
        av_opt_set(codec_ctx_->priv_data, "psy", config_.psy_rd ? "1" : "0", 0);
        
        char psy_rd_str[16];
        snprintf(psy_rd_str, sizeof(psy_rd_str), "%.1f", config_.psy_rd_strength);
        av_opt_set(codec_ctx_->priv_data, "psy-rd", psy_rd_str, 0);
        
        av_opt_set(codec_ctx_->priv_data, "fast_pskip", config_.fast_pskip ? "1" : "0", 0);
        av_opt_set(codec_ctx_->priv_data, "mixed_refs", config_.mixed_refs ? "1" : "0", 0);
        av_opt_set(codec_ctx_->priv_data, "cabac", config_.cabac ? "1" : "0", 0);
        av_opt_set(codec_ctx_->priv_data, "8x8dct", config_.dct8x8 ? "1" : "0", 0);
        av_opt_set(codec_ctx_->priv_data, "aq-mode", config_.aq_mode ? "1" : "0", 0);
        
        char aq_strength_str[16];
        snprintf(aq_strength_str, sizeof(aq_strength_str), "%.1f", config_.aq_strength);
        av_opt_set(codec_ctx_->priv_data, "aq-strength", aq_strength_str, 0);
        
        // Deblocking filter
        codec_ctx_->flags |= config_.deblock ? 0 : AV_CODEC_FLAG_LOOP_FILTER;
        
        char deblock_alpha_str[8], deblock_beta_str[8];
        snprintf(deblock_alpha_str, sizeof(deblock_alpha_str), "%d", config_.deblock_alpha);
        snprintf(deblock_beta_str, sizeof(deblock_beta_str), "%d", config_.deblock_beta);
        
        std::string deblock_str = std::string(deblock_alpha_str) + ":" + std::string(deblock_beta_str);
        av_opt_set(codec_ctx_->priv_data, "deblock", deblock_str.c_str(), 0);
        
        // QP settings
        char qp_min_str[8], qp_max_str[8], qp_step_str[8];
        snprintf(qp_min_str, sizeof(qp_min_str), "%d", config_.qp_min);
        snprintf(qp_max_str, sizeof(qp_max_str), "%d", config_.qp_max);
        snprintf(qp_step_str, sizeof(qp_step_str), "%d", config_.qp_step);
        
        av_opt_set(codec_ctx_->priv_data, "qpmin", qp_min_str, 0);
        av_opt_set(codec_ctx_->priv_data, "qpmax", qp_max_str, 0);
        av_opt_set(codec_ctx_->priv_data, "qpstep", qp_step_str, 0);
        
        // Other settings
        av_opt_set(codec_ctx_->priv_data, "bluray-compat", config_.bluray_compat ? "1" : "0", 0);
        av_opt_set(codec_ctx_->priv_data, "force-cfr", config_.force_cfr ? "1" : "0", 0);
        
        // Intra refresh
        if (config_.intra_refresh > 0) {
            char intra_refresh_str[8];
            snprintf(intra_refresh_str, sizeof(intra_refresh_str), "%d", config_.intra_refresh);
            av_opt_set(codec_ctx_->priv_data, "intra-refresh", intra_refresh_str, 0);
        }
        
        // Keyframe interval in seconds
        if (config_.keyint_sec > 0) {
            char keyint_sec_str[8];
            snprintf(keyint_sec_str, sizeof(keyint_sec_str), "%d", config_.keyint_sec);
            av_opt_set(codec_ctx_->priv_data, "keyint", keyint_sec_str, 0);
        }
        
        // Noise reduction
        if (config_.nr_strength > 0) {
            char nr_str[8];
            snprintf(nr_str, sizeof(nr_str), "%d", config_.nr_strength);
            av_opt_set(codec_ctx_->priv_data, "nr", nr_str, 0);
        }
        
        // Slice max size
        if (config_.slice_max_size > 0) {
            char slice_max_size_str[16];
            snprintf(slice_max_size_str, sizeof(slice_max_size_str), "%d", config_.slice_max_size);
            av_opt_set(codec_ctx_->priv_data, "slice-max-size", slice_max_size_str, 0);
        }
        
        // GOP settings
        av_opt_set(codec_ctx_->priv_data, "open-gop", config_.open_gop ? "1" : "0", 0);
        
        char scenecut_str[8];
        snprintf(scenecut_str, sizeof(scenecut_str), "%d", config_.scenecut_threshold);
        av_opt_set(codec_ctx_->priv_data, "scenecut", scenecut_str, 0);
        
        // Metadata
        av_opt_set(codec_ctx_->priv_data, "repeat-headers", config_.repeat_headers ? "1" : "0", 0);
        av_opt_set(codec_ctx_->priv_data, "annexb", config_.annexb ? "1" : "0", 0);
        
        // Open the codec
        int ret = avcodec_open2(codec_ctx_, codec_, nullptr);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            std::cerr << "Error: Could not open codec: " << errbuf << std::endl;
            Cleanup();
            return false;
        }
        
        // Allocate frame
        frame_ = av_frame_alloc();
        if (!frame_) {
            std::cerr << "Error: Could not allocate frame" << std::endl;
            Cleanup();
            return false;
        }
        
        frame_->format = codec_ctx_->pix_fmt;
        frame_->width = codec_ctx_->width;
        frame_->height = codec_ctx_->height;
        
        // Allocate frame buffer
        ret = av_frame_get_buffer(frame_, 0);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            std::cerr << "Error: Could not allocate frame buffer: " << errbuf << std::endl;
            Cleanup();
            return false;
        }
        
        // Allocate packet
        packet_ = av_packet_alloc();
        if (!packet_) {
            std::cerr << "Error: Could not allocate packet" << std::endl;
            Cleanup();
            return false;
        }
        
        initialized_ = true;
        frame_count_ = 0;
        
        return true;
    }
    
    void Cleanup() {
        if (packet_) {
            av_packet_free(&packet_);
            packet_ = nullptr;
        }
        
        if (frame_) {
            av_frame_free(&frame_);
            frame_ = nullptr;
        }
        
        if (codec_ctx_) {
            avcodec_free_context(&codec_ctx_);
            codec_ctx_ = nullptr;
        }
        
        initialized_ = false;
    }
    
    // Implementation of H264Encoder interface
    
    bool EncodeYUV420(const std::vector<uint8_t>& yuv_data, 
                     std::vector<uint8_t>* output_frame) override {
        if (!initialized_ && !Initialize()) {
            return false;
        }
        
        if (!output_frame) {
            std::cerr << "Error: Output buffer is null" << std::endl;
            return false;
        }
        
        // Check if the input frame has the expected size
        size_t expected_size = config_.width * config_.height * 3 / 2;  // YUV420 format
        if (yuv_data.size() != expected_size) {
            std::cerr << "Error: Invalid YUV data size. Expected " << expected_size
                      << " got " << yuv_data.size() << std::endl;
            return false;
        }
        
        // Make sure the frame is writable
        int ret = av_frame_make_writable(frame_);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            std::cerr << "Error: Could not make frame writable: " << errbuf << std::endl;
            return false;
        }
        
        // Copy YUV data to frame
        // Y plane
        const uint8_t* y_src = yuv_data.data();
        int y_stride = config_.width;
        memcpy(frame_->data[0], y_src, y_stride * config_.height);
        
        // U plane
        const uint8_t* u_src = y_src + (y_stride * config_.height);
        int u_stride = config_.width / 2;
        memcpy(frame_->data[1], u_src, u_stride * (config_.height / 2));
        
        // V plane
        const uint8_t* v_src = u_src + (u_stride * (config_.height / 2));
        int v_stride = config_.width / 2;
        memcpy(frame_->data[2], v_src, v_stride * (config_.height / 2));
        
        // Set presentation timestamp
        frame_->pts = frame_count_++;
        
        return EncodeFrame(frame_, output_frame);
    }
    
    bool Flush(std::vector<uint8_t>* output_frame) override {
        if (!initialized_) {
            return false;
        }
        
        return EncodeFrame(nullptr, output_frame);
    }
    
    bool Reconfigure(const H264EncoderConfig& config) override {
        // Store the new configuration
        config_ = config;
        
        // Re-initialize the encoder with the new configuration
        return Initialize();
    }
    
    H264EncoderConfig GetConfig() const override {
        return config_;
    }
    
private:
    bool EncodeFrame(AVFrame* frame, std::vector<uint8_t>* output_frame) {
        int ret = avcodec_send_frame(codec_ctx_, frame);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            std::cerr << "Error sending frame: " << errbuf << std::endl;
            return false;
        }
        
        output_frame->clear();
        
        while (ret >= 0) {
            ret = avcodec_receive_packet(codec_ctx_, packet_);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                // Need more input or end of stream
                break;
            } else if (ret < 0) {
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                std::cerr << "Error receiving packet: " << errbuf << std::endl;
                return false;
            }
            
            // Append packet data to output
            size_t current_size = output_frame->size();
            output_frame->resize(current_size + packet_->size);
            memcpy(output_frame->data() + current_size, packet_->data, packet_->size);
            
            av_packet_unref(packet_);
        }
        
        return true;
    }
    
    H264EncoderConfig config_;
    bool initialized_;
    int frame_count_;
    
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    AVFrame* frame_ = nullptr;
    AVPacket* packet_ = nullptr;
};

}  // namespace

std::unique_ptr<H264Encoder> H264Encoder::Create(const H264EncoderConfig& config) {
    std::unique_ptr<H264EncoderInstance> encoder = std::make_unique<H264EncoderInstance>(config);
    
    if (!encoder->Initialize()) {
        return nullptr;
    }
    
    return encoder;
}

}  // namespace media