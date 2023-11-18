#pragma once
#include <memory>

#include <boost/system/error_code.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <optional>

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