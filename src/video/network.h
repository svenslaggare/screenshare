#pragma once
#include <memory>
#include <array>
#include <iostream>
#include <optional>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>

#include "common.h"
#include "../misc/time_measurement.h"

namespace screenshare::video::network {
	boost::system::error_code sendAVCodecParameters(boost::asio::ip::tcp::socket& socket, AVCodecParameters& codecParameters);
	boost::system::error_code sendAVCodecParameters(boost::asio::ip::tcp::socket& socket, AVCodecContext* codecContext);

	class AVCodecParametersReceiver {
	private:
		AVCodecParameters mCodecParameters {};
		std::unique_ptr<std::uint8_t[]> mCodecExtraData;
	public:
		explicit AVCodecParametersReceiver(boost::asio::ip::tcp::socket& socket);

		AVCodecParameters* codecParameters();
	};

	class PacketSender {
	public:
		boost::system::error_code send(boost::asio::ip::tcp::socket& socket, AVPacket* packet);

		struct AsyncResult {
			AVPacket packetSerialized {};
			std::array<boost::asio::mutable_buffers_1, 2> buffers;

			boost::system::error_code error;
			std::atomic<bool> done = false;

			explicit AsyncResult(AVPacket* packet);
		};

		using AsyncResultPtr = std::shared_ptr<AsyncResult>;

		AsyncResultPtr sendAsync(boost::asio::ip::tcp::socket& socket, AVPacket* packet);
	};

	class PacketReceiver {
	private:
		std::unique_ptr<AVCodecContext, AVCodecContextDeleter> mCodecContext;
	public:
		explicit PacketReceiver(AVCodecParameters* codecParameters);

		AVCodecContext* codecContext();

		boost::system::error_code receive(boost::asio::ip::tcp::socket& socket, AVPacket* packet);
	};
}