#include "network.h"
#include <boost/algorithm/string.hpp>

namespace screenshare::misc {
	boost::asio::ip::tcp::endpoint tcpEndpointFromString(const std::string& endpoint) {
		std::string delimiter = ":";
		auto pos = endpoint.find(delimiter);
		if (pos != std::string::npos) {
			auto ip = boost::asio::ip::address::from_string(endpoint.substr(0, pos));
			auto port = (std::uint16_t)std::stoi(endpoint.substr(pos + 1));
			return { ip, port };
		} else {
			throw std::runtime_error("Expected ':'.");
		}
	}
}