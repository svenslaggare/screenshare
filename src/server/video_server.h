#pragma once
#include <boost/asio.hpp>
#include "../screengrabber/common.h"

namespace screenshare::video {
	class OutputStream;
	class Converter;

	namespace network {
		class PacketSender;
	}
}

namespace screenshare::screengrabber {
	class GrabbedFrame;
}

namespace screenshare::server {
	class VideoServer {
	private:
		using Socket = boost::asio::ip::tcp::socket;
		using ClientId = std::uint64_t;

		std::atomic<bool> mRun;
		boost::asio::io_context mIOContext;
		boost::asio::ip::tcp::acceptor mAcceptor;
		std::mutex mClientSocketsMutex;
		std::uint64_t mNextClientId = 1;
		std::unordered_map<ClientId, std::shared_ptr<Socket>> mClientSockets;

		bool createFrame(
			video::OutputStream* videoStream,
			video::Converter& converter,
			const screengrabber::GrabbedFrame& grabbedFrame
		);

		using SendResult = std::tuple<ClientId, boost::system::error_code>;
		std::tuple<bool, std::vector<SendResult>> encodeFrameAndSend(
			std::vector<std::tuple<ClientId, Socket*>>& sockets,
			video::OutputStream* videoStream,
			video::network::PacketSender& packetSender
		);

		void accept(video::OutputStream* acceptFailed);
	public:
		explicit VideoServer(boost::asio::ip::tcp::endpoint bind);

		void run(std::unique_ptr<screengrabber::ScreenGrabber> screenGrabber);
	};
}