#pragma once
#include <boost/asio.hpp>

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

		std::atomic<bool> mRun;
		boost::asio::io_context mIOContext;
		boost::asio::ip::tcp::acceptor mAcceptor;
		std::mutex mClientSocketsMutex;
		std::vector<Socket> mClientSockets;

		bool createFrame(
			video::OutputStream* videoStream,
			video::Converter& converter,
			const screengrabber::GrabbedFrame& grabbedFrame
		);

		std::tuple<bool, std::vector<boost::system::error_code>> encodeFrameAndSend(
			std::vector<Socket*>& sockets,
			video::OutputStream* videoStream,
			video::network::PacketSender& packetSender
		);
	public:
		explicit VideoServer(boost::asio::ip::tcp::endpoint bind);

		void run(const std::string& displayName, int windowId);
	};
}