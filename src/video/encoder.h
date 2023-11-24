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
		AVStream* stream = nullptr;
		AVCodec* codec = nullptr;
		std::unique_ptr<AVCodecContext, AVCodecContextDeleter> encoder;

		//pts of the next frame that will be generated
		std::int64_t nextPts = 0;

		std::unique_ptr<AVFrame, AVFrameDeleter> frame;
		std::unique_ptr<AVPacket, AVPacketDeleter> packet;
	};

	class VideoEncoder {
	private:
		std::unique_ptr<AVFormatContext, AVFormatContextDeleter> mOutputFormatContext;
		std::vector<OutputStream> mStreams;
	public:
		explicit VideoEncoder(const std::string& format);

		AVFormatContext* outputFormatContext();
		const std::vector<OutputStream>& streams() const;

		OutputStream* addVideoStream(int width, int height, int frameRate, AVDictionary* options = nullptr);
	};

	std::optional<OutputStream> createOutputStream(AVFormatContext* outputFormatContext, AVCodecID codecId);

	std::optional<OutputStream> createVideoStream(
		AVFormatContext* outputFormatContext, AVCodecID codecId,
		int width, int height, int frameRate, AVDictionary* options
	);
}