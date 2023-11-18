#pragma once
#include <string>

#include <boost/asio.hpp>

namespace screenshare::misc {
	boost::asio::ip::tcp::endpoint tcpEndpointFromString(const std::string& endpoint);
}