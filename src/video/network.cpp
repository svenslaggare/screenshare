#include "decoder.h"
#include "network.h"

namespace screenshare::video::network {
	boost::system::error_code sendAVCodecParameters(boost::asio::ip::tcp::socket& socket, AVCodecParameters& codecParameters) {
		boost::system::error_code error;
		boost::asio::write(
			socket,
			std::array {
				boost::asio::buffer(reinterpret_cast<std::uint8_t*>(&codecParameters), sizeof(codecParameters)),
				boost::asio::buffer(codecParameters.extradata, codecParameters.extradata_size),
			},
			error
		);

		return error;
	}

	boost::system::error_code sendAVCodecParameters(boost::asio::ip::tcp::socket& socket, AVCodecContext* codecContext) {
		AVCodecParameters codecParameters {};
		if (avcodec_parameters_from_context(&codecParameters, codecContext) < 0) {
			throw std::runtime_error("Failed to get codec parameters.");
		}

		return sendAVCodecParameters(socket, codecParameters);
	}

	AVCodecParametersReceiver::AVCodecParametersReceiver(boost::asio::ip::tcp::socket& socket) {
		boost::system::error_code error;
		boost::asio::read(
			socket,
			boost::asio::buffer(reinterpret_cast<std::uint8_t*>(&mCodecParameters), sizeof(mCodecParameters)),
			error
		);
		if (error) {
			throw std::runtime_error("Failed to retrieve codec parameters.");
		}

		mCodecExtraData = std::make_unique<std::uint8_t[]>(mCodecParameters.extradata_size);
		boost::asio::read(
			socket,
			boost::asio::buffer(reinterpret_cast<std::uint8_t*>(mCodecExtraData.get()), mCodecParameters.extradata_size),
			error
		);
		mCodecParameters.extradata = mCodecExtraData.get();
	}

	AVCodecParameters* AVCodecParametersReceiver::codecParameters() {
		return &mCodecParameters;
	}

	namespace {
		AVPacketSerialized serializePacket(const PacketHeader& header, AVPacket* packet) {
			AVPacketSerialized packetSerialized;

			packetSerialized.header = header;

			packetSerialized.packet = *packet;
			packetSerialized.packet.data = nullptr;
			packetSerialized.packet.buf = nullptr;
			packetSerialized.packet.side_data = nullptr;
			packetSerialized.packet.side_data_elems = 0;

			return packetSerialized;
		}
	}

	PacketHeader::PacketHeader(std::int64_t encoderPts)
		: encoderPts(encoderPts) {
		std::timespec_get(&sendTime, TIME_UTC);
	}

	boost::system::error_code PacketSender::send(boost::asio::ip::tcp::socket& socket,
												 const PacketHeader& header,
												 AVPacket* packet) {
		boost::system::error_code error;

		auto packetSerialized = serializePacket(header, packet);
		boost::asio::write(
			socket,
			std::array {
				boost::asio::buffer(reinterpret_cast<std::uint8_t*>(&packetSerialized), sizeof(packetSerialized)),
				boost::asio::buffer(packet->data, packet->size),
			},
			error
		);

		return error;
	}

	PacketSender::AsyncResult::AsyncResult(const PacketHeader& header, AVPacket* packet)
		: packetSerialized(serializePacket(header, packet)),
		  buffers({
			  boost::asio::buffer(reinterpret_cast<std::uint8_t*>(&packetSerialized), sizeof(packetSerialized)),
			  boost::asio::buffer(packet->data, packet->size),
		  }) {

	}

	PacketSender::AsyncResultPtr PacketSender::sendAsync(boost::asio::ip::tcp::socket& socket,
														 const PacketHeader& header,
														 AVPacket* packet) {
		auto asyncResult = std::make_shared<AsyncResult>(header, packet);

		boost::asio::async_write(
			socket,
			asyncResult->buffers,
			[asyncResult](const boost::system::error_code& error, size_t) {
				asyncResult->error = error;
				asyncResult->done = true;
				asyncResult->done.notify_one();
			}
		);

		return asyncResult;
	}

	PacketReceiver::PacketReceiver(AVCodecParameters* codecParameters) {
		auto codec = avcodec_find_decoder(codecParameters->codec_id);
		if (codec == nullptr) {
			throw std::runtime_error("ERROR unsupported codec!");
		}

		mCodecContext = decltype(mCodecContext) { avcodec_alloc_context3(codec) };
		if (!mCodecContext) {
			throw std::runtime_error("Failed to allocated memory for AVCodecContext");
		}

		screenshare::video::handleAVResult(avcodec_parameters_to_context(mCodecContext.get(), codecParameters), "failed to copy codec params to codec context");
		screenshare::video::handleAVResult(avcodec_open2(mCodecContext.get(), codec, nullptr), "failed to open codec through avcodec_open2");
	}

	AVCodecContext* PacketReceiver::codecContext() {
		return mCodecContext.get();
	}

	boost::system::error_code PacketReceiver::receive(boost::asio::ip::tcp::socket& socket,
													  AVPacket* packet,
													  PacketHeader& header) {
		AVPacketSerialized packetSerialized;
		boost::system::error_code error;

		boost::asio::read(
			socket,
			boost::asio::buffer(reinterpret_cast<uint8_t*>(&packetSerialized), sizeof(packetSerialized)),
			error
		);

		if (error) {
			return error;
		}

		if (packet->buf == nullptr || packetSerialized.packet.size > packet->buf->size) {
			av_new_packet(packet, packetSerialized.packet.size);
			packet->data = packet->buf->data;
		}

		boost::asio::read(
			socket,
			boost::asio::buffer(packet->data, packetSerialized.packet.size),
			error
		);

		header = packetSerialized.header;

		auto buf = packet->buf;
		*packet = packetSerialized.packet;
		packet->buf = buf;
		packet->data = packet->buf->data;

		return {};
	}
}