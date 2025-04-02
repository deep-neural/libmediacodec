#ifndef MEDIA_OPUS_ENCODER_H_
#define MEDIA_OPUS_ENCODER_H_

#include <cstdint>
#include <memory>
#include <vector>
#include <string>

namespace media {

// OPUS application types
enum class OPUSApplication {
    VOIP = 2048,       // Optimize for voice, low-delay communication
    AUDIO = 2049,      // Optimize for general audio
    RESTRICTED_LOWDELAY = 2051  // Optimize for low-delay real-time applications
};

struct OPUSEncoderConfig {
    // Basic parameters
    int sample_rate = 48000;       // Valid values: 8000, 12000, 16000, 24000, 48000 Hz
    int channels = 2;              // Valid values: 1 (mono), 2 (stereo)
    int bitrate = 96000;           // Bitrate in bps (6000 to 510000)
    
    // Application type
    OPUSApplication application = OPUSApplication::AUDIO;
    
    // Frame size (duration) in milliseconds
    // Valid values: 2.5, 5, 10, 20, 40, 60, 80, 100, 120
    int frame_duration_ms = 20;
    
    // Encoding complexity (0-10, higher is more complex but better quality)
    int complexity = 10;
    
    // Signal type hints
    bool use_inband_fec = false;   // Forward error correction
    bool use_dtx = false;          // Discontinuous transmission (silence encoding)
    
    // Bandwidth control
    enum class Bandwidth {
        NARROWBAND = 1101,         // 4 kHz passband
        MEDIUMBAND = 1102,         // 6 kHz passband
        WIDEBAND = 1103,           // 8 kHz passband
        SUPERWIDEBAND = 1104,      // 12 kHz passband
        FULLBAND = 1105            // 20 kHz passband
    };
    Bandwidth bandwidth = Bandwidth::FULLBAND;
    
    // VBR settings
    bool use_vbr = true;           // Variable bit rate
    bool use_cvbr = true;          // Constrained VBR
    
    // Packet loss resilience
    int packet_loss_percentage = 0; // Expected packet loss percentage (0-100)
    
    // Signal type hint
    enum class SignalType {
        AUTO = -1000,               // Automatic detection
        VOICE = 3001,               // Voice signal
        MUSIC = 3002                // Music signal
    };
    SignalType signal_type = SignalType::AUTO;
    
    // Expert frame control
    int max_frame_size_ms = 120;    // Maximum frame size in milliseconds
    int min_frame_size_ms = 2;      // Minimum frame size in milliseconds
    
    // LSB depth for bit-exact encoding (8-24)
    int lsb_depth = 16;
    
    // Prediction control
    enum class PredictionDisabled {
        DEFAULT = -1000,           // Use default prediction
        NO = 0,                    // Enable prediction
        YES = 1                    // Disable prediction
    };
    PredictionDisabled prediction_disabled = PredictionDisabled::DEFAULT;
};

class OPUSEncoder {
public:
    // Factory method to create an encoder with the specified configuration
    static std::unique_ptr<OPUSEncoder> Create(const OPUSEncoderConfig& config);
    
    // Virtual destructor for proper cleanup
    virtual ~OPUSEncoder() = default;
    
    // Encode PCM audio data in various formats
    virtual int EncodePCM_S16LE(const std::vector<uint8_t>& pcm_data, std::vector<uint8_t>* frame) = 0;
    virtual int EncodePCM_U8(const std::vector<uint8_t>& pcm_data, std::vector<uint8_t>* frame) = 0;
    virtual int EncodePCM_F32BE(const std::vector<uint8_t>& pcm_data, std::vector<uint8_t>* frame) = 0;
    
    // Get the last error as a string
    virtual std::string GetLastError() const = 0;
};

}  // namespace media

#endif  // MEDIA_OPUS_ENCODER_H_