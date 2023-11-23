#pragma once
#include <boost/asio.hpp>

#include "../screeninteractor//common.h"
#include "../client/actions.h"
#include "../misc/concurrency.hpp"

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

		std::atomic<bool> mRun;
		boost::asio::io_context mIOContext;
		boost::asio::ip::tcp::acceptor mAcceptor;
		std::mutex mClientSocketsMutex;
		std::uint64_t mNextClientId = 1;
		std::unordered_map<ClientId, std::shared_ptr<Socket>> mClientSockets;

		misc::ResourceMutex<std::vector<client::ClientAction>> mClientActions;

		bool createFrame(
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
		void receiveFromClient(std::shared_ptr<Socket> socket);
	public:
		explicit VideoServer(boost::asio::ip::tcp::endpoint bind);

		void run(std::unique_ptr<screeninteractor::ScreenInteractor> screenInteractor);
	};
}