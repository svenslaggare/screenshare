#pragma once

#include <memory>
#include <optional>
#include <iostream>
#include <vector>
#include <sstream>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>

#include "common.h"

namespace screenshare::video {
	class Converter {
	private:
		std::unique_ptr<SwsContext, SwsContextDeleter> mConversion;
	public:
		bool convert(
			int sourceWidth, int sourceHeight, AVPixelFormat sourceFormat, std::uint8_t* source, int sourceLineSize,
			int destinationWidth, int destinationHeight, AVPixelFormat destinationFormat, std::uint8_t** destination, int* destinationLineSize
		);
	};

	struct OutputStream {
		AVCodec* codec = nullptr;
		AVStream* stream = nullptr;
		std::unique_ptr<AVCodecContext, AVCodecContextDeleter> encoder;

		std::int64_t nextPts = 0;

		std::unique_ptr<AVFrame, AVFrameDeleter> frame;
		std::unique_ptr<AVPacket, AVPacketDeleter> packet;

		OutputStream(AVCodec* codec, AVStream* stream);

		static std::optional<OutputStream> create(AVFormatContext* outputFormatContext, AVCodecID codecId);
	};

	struct VideoEncoderConfig {
		int width = 0;
		int height = 0;
		int frameRate = 0;
	};

	class VideoEncoder {
	private:
		std::unique_ptr<AVFormatContext, AVFormatContextDeleter> mOutputFormatContext;
		std::vector<OutputStream> mStreams;
	public:
		explicit VideoEncoder(const std::string& format);

		AVFormatContext* outputFormatContext();
		const std::vector<OutputStream>& streams() const;

		OutputStream* addVideoStream(const VideoEncoderConfig& config, AVDictionary* options = nullptr);
	};

	std::optional<OutputStream> createVideoStream(
		AVFormatContext* outputFormatContext, AVCodecID codecId,
		const VideoEncoderConfig& config, AVDictionary* options
	);
}