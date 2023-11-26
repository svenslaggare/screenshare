#pragma once
#include <thread>

#include <boost/asio.hpp>

#include "../screeninteractor//common.h"
#include "../client/actions.h"
#include "../misc/concurrency.hpp"
#include "../video/encoder.h"

namespace screenshare::video {
	class OutputStream;
	class Converter;

	namespace network {
		class PacketSender;
	}
}

namespace screenshare::screeninteractor {
	class GrabbedFrame;
}

namespace screenshare::server {
	class VideoServer {
	private:
		using Socket = boost::asio::ip::tcp::socket;
		using ClientId = std::uint64_t;

		video::VideoEncoder mVideoEncoder;
		video::OutputStream* mVideoStream;

		boost::asio::io_context mIOContext;
		std::jthread mIOContextThread;
		boost::asio::ip::tcp::acceptor mAcceptor;

		std::uint64_t mNextClientId = 1;
		misc::ResourceMutex<std::unordered_map<ClientId, std::shared_ptr<Socket>>> mClientSockets;

		misc::ResourceMutex<std::vector<client::ClientAction>> mClientActions;

		bool nextFrame(
			video::OutputStream* videoStream,
			video::Converter& converter,
			const screeninteractor::GrabbedFrame& grabbedFrame
		);

		using SendResult = std::tuple<ClientId, boost::system::error_code>;
		std::tuple<bool, std::vector<SendResult>> encodeFrameAndSend(
			std::vector<std::tuple<ClientId, Socket*>>& sockets,
			video::OutputStream* videoStream,
			video::network::PacketSender& packetSender
		);

		void accept(video::OutputStream* acceptFailed);
		void receiveFromClient(
			std::shared_ptr<Socket> socket,
			std::shared_ptr<client::ClientAction> clientAction
		);
	public:
		explicit VideoServer(boost::asio::ip::tcp::endpoint bind, video::VideoEncoderConfig videoEncoderConfig);

		void run(std::unique_ptr<screeninteractor::ScreenInteractor> screenInteractor);
		void stop();
	};
}