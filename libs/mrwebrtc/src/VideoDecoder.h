#pragma once

#include <thread>
#include <queue>
#include <media/NdkMediaCodec.h>
#include <api/video_codecs/video_decoder_factory.h>
#include <api/video_codecs/video_decoder.h>
#include <modules/video_coding/include/video_error_codes.h>
#include <android/log.h>
namespace isar
{
#define LOG_TAG "isar-client"
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__))

class FrameBufferCallback;

class VideoDecoder : public webrtc::VideoDecoder {
public:
	int32_t InitDecode(const webrtc::VideoCodec* codecSettings, int32_t numberOfCores); //MY
    int32_t Release() override;

    int32_t RegisterDecodeCompleteCallback(webrtc::DecodedImageCallback* callback) override {
		decodedImageCallback_ = callback;
		return WEBRTC_VIDEO_CODEC_OK;
	}

	int32_t Decode(const webrtc::EncodedImage& inputImage, bool missingFrames,const webrtc::CodecSpecificInfo* codec_specific_info, int64_t renderTimeMs) ;// MY

    const char* ImplementationName() const override {
		return "IsarH264Decoder";
	}

private:
	void outputProcessor();

	const static int64_t COLOR_FORMAT =   19; //35; //COLOR_FormatYUV420Flexible //6; // COLOR_Format16bitRGB565
	const static int64_t DEQUEUE_INPUT_KEY_TIMEOUT_US = 500000;
	const static int64_t DEQUEUE_INPUT_DELTA_TIMEOUT_US = 5000;
	const static int64_t DEQUEUE_OUTPUT_BUFFER_TIMEOUT_US = 500;

	struct FrameInfo {
		FrameInfo(int64_t decodeStartIimeMs, uint32_t timestampFrame, webrtc::XRTimestamp xrTimeStamp)
			: decodeStartIimeMs_(decodeStartIimeMs)
			, timestampFrame_(timestampFrame)
			, XRTimeStamp_(xrTimeStamp)
		{}
		int64_t decodeStartIimeMs_;
		uint32_t timestampFrame_;
		webrtc::XRTimestamp XRTimeStamp_;
	};
	typedef std::unique_ptr<FrameInfo> FrameInfoPtr;
	typedef std::queue<FrameInfoPtr> FrameInfos;

	webrtc::DecodedImageCallback* decodedImageCallback_ = nullptr;
	webrtc::VideoCodec codecSettings_;
	AMediaCodec* codec_ = nullptr;
	std::atomic_bool running_ = {false};
	std::thread outputThread_;
	FrameInfos frameInfos_;
	std::mutex frameInfoMutex_;
    std::mutex mutex_;
    size_t decodedFrameSize;
};

class VideoDecoderFactory : public webrtc::VideoDecoderFactory {
public:
	VideoDecoderFactory(){}
    std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(const webrtc::SdpVideoFormat& format) override;
    std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;
};

}  // namespace isar
