#include <modules/video_coding/codecs/h264/include/h264.h>
#include "VideoEncoder.h"

namespace isar
{

std::vector<webrtc::SdpVideoFormat> VideoEncoderFactory::GetSupportedFormats() const
{
    std::vector<webrtc::SdpVideoFormat> formats = {
            webrtc::SdpVideoFormat(cricket::kH264CodecName,
                           {
                                   //copy-pasted from h264.cc
                                   {cricket::kH264FmtpProfileLevelId, "42100b"},
                                   {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
                                   {cricket::kH264FmtpPacketizationMode, "0"}
                           }),
            webrtc::SdpVideoFormat(cricket::kH264CodecName,
                           {
                                   {cricket::kH264FmtpProfileLevelId, "42100b"},
                                   {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
                                   {cricket::kH264FmtpPacketizationMode, "1"}
                           }),
            // These are needed for android to be able to connect. I think it's H264 CBP
            // (constrained baseline profile) vs BP (baseline profile) though I don't know
            // which is which.
            webrtc::SdpVideoFormat(cricket::kH264CodecName,
                           {
                                   {cricket::kH264FmtpProfileLevelId, "42e01f"},
                                   {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
                                   {cricket::kH264FmtpPacketizationMode, "0"}
                           }),
            webrtc::SdpVideoFormat(cricket::kH264CodecName,
                           {
                                   {cricket::kH264FmtpProfileLevelId, "42e01f"},
                                   {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
                                   {cricket::kH264FmtpPacketizationMode, "1"}
                           })
    };
    return formats;
}

std::unique_ptr<webrtc::VideoEncoder> VideoEncoderFactory::CreateVideoEncoder(const webrtc::SdpVideoFormat& format)
{
    return std::make_unique<isar::VideoEncoder>();
}
VideoEncoderFactory::CodecInfo isar::VideoEncoderFactory::QueryVideoEncoder(const webrtc::SdpVideoFormat& format) const
{
    VideoEncoderFactory::CodecInfo info;
    //not sure about this. mf does support hw MFTs but doesn't really tell us
    //when using sink writer. it's more of a "silent sw fallback if hw not available" thing
    info.is_hardware_accelerated = false;
    info.has_internal_source = false;
    return info;
}

}  // namespace isar
