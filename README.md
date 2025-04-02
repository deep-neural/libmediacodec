<h1 align="center">
  <a href="https://github.com/deep-neural/libmediacodec"><img src="./.github/logo.jpg" alt="libMediaCodec" height="150px"></a>
  <br>
  libMediaCodec
  <br>
</h1>
<h4 align="center">An advanced C/C++ library for accelerated media encoding and decoding supporting modern codecs and GPU acceleration</h4>
<p align="center">
    <a href="https://github.com/deep-neural/libmediacodec"><img src="https://img.shields.io/badge/libMediaCodec-C/C++-blue.svg?longCache=true" alt="libMediaCodec" /></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-5865F2.svg" alt="License: MIT" /></a>
    <br>
    <a href="https://github.com/deep-neural/libmediacodec"><img src="https://img.shields.io/static/v1?label=Build&message=Documentation&color=brightgreen" /></a>
</p>
<br>

### New Release

libMediaCodec v1.0.0 has been released! See the [release notes](https://github.com/deep-neural/libmediacodec/) to learn about new features, enhancements, and breaking changes.

If you aren’t ready to upgrade yet, check the [tags](https://github.com/deep-neural/libmediacodec/) for previous stable releases.

We appreciate your feedback! Feel free to open GitHub issues or submit changes to stay updated in development and connect with the maintainers.

-----

### Usage

libMediaCodec is distributed as a pure C/C++ library. To integrate it into your project, ensure you have a compatible C/C++ compiler and the necessary build tools. Clone the repository and link against the library in your build system.

## Simple API
<table>
<tr>
<th> Encoding </th>
<th> Decoding </th>
</tr>
<tr>
<td>

```cpp
#include <mediacodec.h>

media::VideoEncoderConfig config {
    .gpu_acceleration = true,
    .input_format = media::PixelFormat::NV12,
    .output_codec = media::CodecType::AV1,
    .params = {
        .width = 1920,
        .height = 1080,
        .bitrate = 5000000,
        .framerate = 60,
    }
};

auto encoder = media::VideoEncoder::Create(config);
if(!encoder) { 

}

std::vector<uint8_t> yuv_data;
std::vector<uint8_t> av1_frame;

int success = encoder->Encode(yuv_data, &av1_frame);
if(!success) {

}
```

</td>
<td>

```cpp
#include <mediacodec.h>

media::VideoDecoderConfig dec_cfg {
    .gpu_acceleration = true,
    .input_codec = media::PixelFormat::NV12,
    .output_format = media::PixelFormat::AV1
};

auto decoder = media::VideoDecoder::Create(dec_cfg);
if(!decoder) { 

}

std::vector<uint8_t> av1_frame;
std::vector<uint8_t> yuv_data;

int success = decoder->Decode(av1_frame, &yuv_data);
if(!success) {

}
```

</td>
</tr>
</table>

**[Example Applications](examples/README.md)** contain code samples demonstrating common use cases with libMediaCodec.

**[API Documentation](https://github.com/deep-neural/libmediacodec/docs)** provides a comprehensive reference of our Public APIs.

Now go build something amazing! Here are some ideas to spark your creativity:
* Accelerate media streaming services with optimized encoding and decoding.
* Develop a real-time video processing application with GPU-accelerated codec support.
* Create high-performance media servers utilizing modern codec standards.
* Implement fast transcoding systems for large video libraries.
* Integrate advanced video effects and filters leveraging GPU capabilities.

## Building

See [BUILDING.md](https://github.com/deep-neural/libmediacodec/blob/master/BUILDING.md) for building instructions.

### Features

#### Modern Codec Support
* Supports a wide range of modern codecs: AV1, H264, HEVC, Opus, VP8, VP9.
* Seamless GPU acceleration for both encoding and decoding tasks.

#### Optimized Performance
* Built for speed and efficiency to handle large media files effortlessly.
* Parallel processing capabilities to maximize hardware utilization.

#### Flexible API
* Simplified API design for easy integration and use.
* Customizable parameters for fine-tuning performance and output.

#### Security
* Comprehensive security features ensuring data integrity and protection.
* Regular updates to mitigate emerging vulnerabilities.

#### Pure C/C++
* Written entirely in C/C++ with minimal dependencies for maximum performance.
* Wide platform support: Windows, macOS, Linux, FreeBSD, and more.

### Contributing

Check out the [contributing guide](https://github.com/deep-neural/libmediacodec/wiki/Contributing) to join the team of dedicated contributors making this project possible.

### License

MIT License - see [LICENSE](LICENSE) for full text
