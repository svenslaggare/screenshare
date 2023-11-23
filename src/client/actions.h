#pragma once

#include <string>

#include <boost/asio.hpp>

namespace screenshare::client {
	enum class ClientActionType {
		NoAction = 0,
		KeyPressed
	};

	struct KeyPressedClientAction {
		char key[4] = {};
		static KeyPressedClientAction fromString(const std::string& keyString);
	};

	struct ClientAction {
		ClientActionType type = ClientActionType::NoAction;
		union {
			KeyPressedClientAction keyPress;
		} data = {};

		static ClientAction keyPressed(const std::string& key);

		boost::system::error_code send(boost::asio::ip::tcp::socket& socket);

		static void receiveAsync(
			std::shared_ptr<boost::asio::ip::tcp::socket> socket,
			std::function<void (boost::system::error_code, std::shared_ptr<ClientAction>)> callback
		);
	};
}