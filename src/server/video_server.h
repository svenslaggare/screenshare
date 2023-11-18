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
		boost::asio::io_context mIOContext;
		boost::asio::ip::tcp::acceptor mAcceptor;

		bool createFrame(
			video::OutputStream* videoStream,
			video::Converter& converter,
			const screengrabber::GrabbedFrame& grabbedFrame
		);

		bool encodeFrameAndSend(
			boost::asio::ip::tcp::socket& socket,
			video::OutputStream* videoStream,
			video::network::PacketSender& packetSender
		);
	public:
		explicit VideoServer(boost::asio::ip::tcp::endpoint bind);

		void run(const std::string& displayName, int windowId);
	};
}