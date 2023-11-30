#include <chrono>
#include <modules/video_coding/codecs/h264/include/h264.h>
#include <api/video/i420_buffer.h>
#include "VideoDecoder.h"
#include <libyuv.h>

namespace isar
{

inline int64_t current_time_ms()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

int32_t VideoDecoder::InitDecode(const webrtc::VideoCodec* codecSettings, int32_t numberOfCores)
{
	codecSettings_ = *codecSettings;
	const char* mime_type = "video/avc";
	codec_ = AMediaCodec_createDecoderByType(mime_type);
	AMediaFormat* format = AMediaFormat_new();
	AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime_type);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, codecSettings->width);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, codecSettings->height);
	AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, COLOR_FORMAT);
	AMediaCodec_configure(codec_, format, nullptr, nullptr, 0);
	AMediaCodec_start(codec_);
	AMediaFormat_delete(format);
	running_ = true;
	outputThread_ = std::thread(&VideoDecoder::outputProcessor, this);
    decodedFrameSize = codecSettings->width * codecSettings->height * 1.5;
	return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VideoDecoder::Release()
{
	AMediaCodec_stop(codec_);
	AMediaCodec_delete(codec_);
	running_ = false;
	outputThread_.join();
	FrameInfos empty;
	std::swap(frameInfos_, empty);
	codec_ = nullptr;
	decodedImageCallback_ = nullptr;
	return WEBRTC_VIDEO_CODEC_OK;
}

int32_t VideoDecoder::Decode(const webrtc::EncodedImage& inputImage, bool missingFrames,const webrtc::CodecSpecificInfo* codec_specific_info, int64_t renderTimeMs)
{
	if (!codec_ || !decodedImageCallback_) {
        //LOGV("Decode not initialized");
		return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
	}
    //LOGV("VideoDecoder: decode %dx%d", codecSettings_.width, codecSettings_.height);
	ssize_t bufidx = AMediaCodec_dequeueInputBuffer(codec_,
			 inputImage._frameType == webrtc::VideoFrameType::kVideoFrameKey ? DEQUEUE_INPUT_KEY_TIMEOUT_US : DEQUEUE_INPUT_DELTA_TIMEOUT_US); //MY
	if (bufidx >= 0) {
		size_t bufsize;
		auto buffer = AMediaCodec_getInputBuffer(codec_, bufidx, &bufsize);
		if (bufsize < inputImage.size()) { //MY
            //LOGV("VideoDecoder: input buffer size %zd less than image size %zd", bufsize, inputImage._size);
			return WEBRTC_VIDEO_CODEC_ERROR;
		}
		memcpy(buffer, inputImage.data(), inputImage.size()); //MY
		{
			std::unique_lock<std::mutex> locker(frameInfoMutex_);
			frameInfos_.push(std::make_unique<FrameInfo>(current_time_ms(), inputImage.Timestamp(), inputImage.xr_timestamp_));
		}
		AMediaCodec_queueInputBuffer(codec_, bufidx, 0, inputImage.size(), inputImage.ntp_time_ms_, 0); //MY
	}
	else {
        //LOGV("VideoDecoder: dequeueInputBuffer no input buffer available");
		return WEBRTC_VIDEO_CODEC_ERROR;
	}
	return WEBRTC_VIDEO_CODEC_OK;
}

void VideoDecoder::outputProcessor()
{
	while (running_) {
		AMediaCodecBufferInfo info;
		std::queue<uint32_t> buffer_queue;
		ssize_t current_bufidx;
		do {
			current_bufidx = AMediaCodec_dequeueOutputBuffer(codec_, &info, DEQUEUE_OUTPUT_BUFFER_TIMEOUT_US);
			if (current_bufidx >= 0){
				if (info.flags & (uint32_t)AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) continue;
				buffer_queue.push(current_bufidx);
			}
		} while (current_bufidx >= 0);

		if (buffer_queue.size() != 0) {
			//discard all frames except the last one
			while (buffer_queue.size() > 1) {
				{
					std::unique_lock<std::mutex> locker(frameInfoMutex_);
					frameInfos_.pop();
				}
				AMediaCodec_releaseOutputBuffer(codec_, buffer_queue.front(), false);
				buffer_queue.pop();
			}

			size_t bufferSize;
			rtc::scoped_refptr<webrtc::I420Buffer> myBuffer = webrtc::I420Buffer::Create(codecSettings_.width, codecSettings_.height);
			uint8_t *buffer = AMediaCodec_getOutputBuffer(codec_, buffer_queue.front(), &bufferSize);

			std::memcpy(myBuffer->MutableDataY(), buffer, decodedFrameSize);

			FrameInfoPtr frameInfo;
			{
				std::unique_lock<std::mutex> locker(frameInfoMutex_);
				frameInfo = std::move(frameInfos_.front());
				frameInfos_.pop();
			}

			if (decodedImageCallback_) {
				webrtc::VideoFrame vf(myBuffer,frameInfo->timestampFrame_, 0, webrtc::kVideoRotation_0);
                vf.set_xr_timestamp(frameInfo->XRTimeStamp_);

				decodedImageCallback_->Decoded(vf);
                //LOGV("VideoDecoder: frameInfo->XRTimeStamp_ %li", frameInfo->XRTimeStamp_);
				/*frameBufferCallback_->onFrameBuffer(codecSettings_.width, codecSettings_.height,
													buffer, bufferSize,
													frameInfo->XRTimeStamp_);*/
			}

			AMediaCodec_releaseOutputBuffer(codec_, buffer_queue.front(), false);
			buffer_queue.pop();
		} else {
			if (current_bufidx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
				//LOGV("VideoDecoder: output buffers changed");
			} else if (current_bufidx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
				auto format = AMediaCodec_getOutputFormat(codec_);
				//LOGV("VideoDecoder: format changed to: %s", AMediaFormat_toString(format));
				AMediaFormat_delete(format);
			} else if (current_bufidx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
                //LOGV("VideoDecoder: no output buffer right now");
			} else {
                //LOGV("VideoDecoder: unexpected info code: %zd", current_bufidx);
				break;
			}
		}
	}
}

std::vector<webrtc::SdpVideoFormat> VideoDecoderFactory::GetSupportedFormats() const
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

std::unique_ptr<webrtc::VideoDecoder> VideoDecoderFactory::CreateVideoDecoder(const webrtc::SdpVideoFormat& format)
{
	//LOGV("Create Decoder format %s", format.ToString().c_str());
	//videoDecoder->RegisterFrameBufferCallback(frameBufferCallback_);
	return std::make_unique<isar::VideoDecoder>();
}

}  // namespace isar
