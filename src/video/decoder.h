#pragma once
#include <memory>
#include <iostream>
#include <optional>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>

#include "common.h"
#include "../misc/time_measurement.h"

namespace screenshare::video {
	class PacketDecoder {
	private:
		std::unique_ptr<SwsContext, SwsContextDeleter> mConversion;
	public:
		int decode(
			AVPacket* packet,
			AVCodecContext* codecContext,
			AVFrame* frame,
			std::uint8_t* destination,
			std::function<void (AVCodecContext* codecContext)> callback
		);
	};
}