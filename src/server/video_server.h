#pragma once
#include <boost/asio.hpp>

namespace screenshare::server {
	class VideoServer {
	private:
		boost::asio::io_context mIOContext;
		boost::asio::ip::tcp::acceptor mAcceptor;
	public:
		explicit VideoServer(boost::asio::ip::tcp::endpoint bind);

		void run();
	};
}