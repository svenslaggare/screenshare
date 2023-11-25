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
	struct CustomCodecParameters {
		AVRational timeBase { 0, 0 };
	};

	boost::system::error_code sendAVCodecParameters(boost::asio::ip::tcp::socket& socket, AVCodecContext* codecContext);

	class AVCodecParametersReceiver {
	private:
		CustomCodecParameters mCustomCodecParameters;
		AVCodecParameters mCodecParameters {};
		std::unique_ptr<std::uint8_t[]> mCodecExtraData;
	public:
		explicit AVCodecParametersReceiver(boost::asio::ip::tcp::socket& socket);

		AVCodecParameters* codecParameters();

		AVRational timeBase() const;
	};

	struct PacketHeader {
		std::int64_t encoderPts = 0;
		std::timespec sendTime {};

		PacketHeader() = default;
		explicit PacketHeader(std::int64_t encoderPts);
	};

	struct AVPacketSerialized {
		PacketHeader header;
		AVPacket packet = {};
	};

	class PacketSender {
	public:
		boost::system::error_code send(
			boost::asio::ip::tcp::socket& socket,
			const PacketHeader& header,
			AVPacket* packet
		);

		struct AsyncResult {
			AVPacketSerialized packetSerialized {};
			std::array<boost::asio::mutable_buffers_1, 2> buffers;

			boost::system::error_code error;
			std::atomic<bool> done = false;

			explicit AsyncResult(const PacketHeader& header, AVPacket* packet);
		};

		using AsyncResultPtr = std::shared_ptr<AsyncResult>;

		AsyncResultPtr sendAsync(
			boost::asio::ip::tcp::socket& socket,
			const PacketHeader& header,
			AVPacket* packet
		);
	};

	class PacketReceiver {
	private:
		std::unique_ptr<AVCodecContext, AVCodecContextDeleter> mCodecContext;
	public:
		explicit PacketReceiver(AVCodecParameters* codecParameters);

		AVCodecContext* codecContext();

		boost::system::error_code receive(
			boost::asio::ip::tcp::socket& socket,
			AVPacket* packet,
			PacketHeader& header
		);
	};
}