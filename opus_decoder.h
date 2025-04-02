// opus_decoder.h
#ifndef MEDIA_OPUS_DECODER_H_
#define MEDIA_OPUS_DECODER_H_

#include <cstdint>
#include <memory>
#include <vector>

namespace media {

// Configuration options for the OPUS decoder
struct OPUSDecoderConfig {
  // Sample rate of the output audio (default: 48000Hz which is common for OPUS)
  // Valid values: 8000, 12000, 16000, 24000, 48000
  int sample_rate = 48000;
  
  // Number of channels in the output audio (default: 2 for stereo)
  // Valid values: 1 (mono), 2 (stereo)
  int channels = 2;
  
  // Apply gain in dB to decoded output (default: 0)
  // Range: -32768 to 32767
  int gain_db = 0;
  
  // Enable Forward Error Correction (default: false)
  bool use_fec = false;
  
  // Enable Discontinuous Transmission (default: false)
  bool use_dtx = false;
  
  // Maximum packet loss percentage to recover with FEC (default: 0)
  // Range: 0 to 100
  int packet_loss_percentage = 0;
  
  // Low-latency mode reducing algorithmic delay (default: false)
  bool low_latency_mode = false;
  
  // Constrained Variable Bit Rate mode (default: false)
  bool constrained_vbr = false;
  
  // Maximum bandwidth to decode (default: fullband)
  enum class Bandwidth {
    NARROWBAND,     // 4kHz
    MEDIUMBAND,     // 6kHz
    WIDEBAND,       // 8kHz
    SUPERWIDEBAND,  // 12kHz
    FULLBAND        // 20kHz
  };
  Bandwidth max_bandwidth = Bandwidth::FULLBAND;
  
  // Output frame size in milliseconds (default: 20ms)
  // Valid values: 2.5, 5, 10, 20, 40, 60, 80, 100, 120
  double frame_size_ms = 20.0;
  
  // Buffer for concealing lost packets (default: 5 frames)
  int plc_buffer_size = 5;
};

// Class for decoding OPUS audio frames to PCM formats
class OPUSDecoder {
 public:
  // Creates a new OPUSDecoder with the specified configuration
  static std::unique_ptr<OPUSDecoder> Create(const OPUSDecoderConfig& config);
  
  // Destructor
  virtual ~OPUSDecoder() = default;

  // Decodes an OPUS frame to 16-bit signed little-endian PCM
  // Returns 1 on success, 0 on failure
  virtual int DecodeToPCM_S16LE(const std::vector<uint8_t>& opus_frame,
                               std::vector<uint8_t>* pcm_frame) = 0;
  
  // Decodes an OPUS frame to 8-bit unsigned PCM
  // Returns 1 on success, 0 on failure
  virtual int DecodeToPCM_U8(const std::vector<uint8_t>& opus_frame,
                            std::vector<uint8_t>* pcm_frame) = 0;
  
  // Decodes an OPUS frame to 32-bit float big-endian PCM
  // Returns 1 on success, 0 on failure
  virtual int DecodeToPCM_F32BE(const std::vector<uint8_t>& opus_frame,
                               std::vector<uint8_t>* pcm_frame) = 0;
                               
  // Updates decoder configuration parameters
  // Returns true on success, false on failure
  virtual bool UpdateConfig(const OPUSDecoderConfig& config) = 0;
  
  // Gets the last error message
  virtual const char* GetLastError() const = 0;
  
  // Resets the decoder state
  virtual void Reset() = 0;
};

}  // namespace media

#endif  // MEDIA_OPUS_DECODER_H_