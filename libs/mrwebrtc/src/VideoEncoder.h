#pragma once

#include <api/video_codecs/video_encoder_factory.h>
#include <modules/video_coding/codecs/h264/include/h264.h>

namespace isar
{

    class VideoEncoder : public webrtc::H264Encoder
    {
    public:
        int InitEncode(const webrtc::VideoCodec* codec_settings,
                       int number_of_cores, size_t max_payload_size) override {}
        int RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override {}
        int Release() override {}
        int Encode(const webrtc::VideoFrame& input_image,
                   const webrtc::CodecSpecificInfo* codec_specific_info,
                   const std::vector<webrtc::FrameType>* frame_types) override {}
        int SetChannelParameters(uint32_t packet_loss, int64_t rtt) override {}
        //void SetRates(const webrtc::RateControlParameters& parameters) override {}
        //webrtc::ScalingSettings GetScalingSettings() const override {}
        //const char* ImplementationName() const override {}
    };

class VideoEncoderFactory : public webrtc::VideoEncoderFactory {
public:
    std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(const webrtc::SdpVideoFormat& format) override;
    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
    CodecInfo QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const override;
};

}  // namespace isar
