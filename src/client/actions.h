#pragma once

#include <string>

#include <boost/asio.hpp>

namespace screenshare::client {
	enum class ClientActionType {
		NoAction = 0,
		KeyPressed,
		MouseButtonPressed
	};

	struct KeyPressedClientAction {
		char key[4] = {};
	};

	struct MouseButtonPressedClientAction {
		std::uint32_t mouseButton = 0;
		double x = 0.0;
		double y = 0.0;
	};

	struct ClientAction {
		ClientActionType type = ClientActionType::NoAction;
		union {
			KeyPressedClientAction keyPressed;
			MouseButtonPressedClientAction mouseButtonPressed;
		} data = {};

		void clear();

		std::string toString() const;

		static ClientAction keyPressed(const std::string& key);
		static ClientAction mouseButtonPressed(std::uint32_t mouseButton, double x, double y);

		boost::system::error_code send(boost::asio::ip::tcp::socket& socket);

		static void receiveAsync(
			std::shared_ptr<boost::asio::ip::tcp::socket> socket,
			std::shared_ptr<ClientAction> clientAction,
			std::function<void (boost::system::error_code, std::shared_ptr<ClientAction>)> callback
		);
	};
}