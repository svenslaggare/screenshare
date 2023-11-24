#include "actions.h"

#include <fmt/format.h>

namespace screenshare::client {
	void ClientAction::clear() {
		*this = {};
	}

	std::string ClientAction::toString() const {
		switch (type) {
			case ClientActionType::NoAction:
				return "NoAction";
			case ClientActionType::KeyPressed:
				return fmt::format("KeyPressed(key: {})", data.keyPressed.key);
			case ClientActionType::MouseButtonPressed:
				return fmt::format(
					"MouseButtonPressed(button: {}, x: {}, y: {})",
					data.mouseButtonPressed.mouseButton,
					data.mouseButtonPressed.x,
					data.mouseButtonPressed.y
				);
			default:
				return "";
		}
	}

	ClientAction ClientAction::keyPressed(const std::string& key) {
		KeyPressedClientAction action;
		for (std::size_t i = 0; i < std::min<std::size_t>(key.size(), 4); i++) {
			action.key[i] = key[i];
		}

		return ClientAction {
			.type = client::ClientActionType::KeyPressed,
			.data = {
				.keyPressed = action
			}
		};
	}

	ClientAction ClientAction::mouseButtonPressed(std::uint32_t mouseButton, double x, double y) {
		return ClientAction {
			.type = client::ClientActionType::MouseButtonPressed,
			.data = {
				.mouseButtonPressed = client::MouseButtonPressedClientAction { mouseButton, x, y }
			}
		};
	}

	boost::system::error_code ClientAction::send(boost::asio::ip::tcp::socket& socket) {
		boost::system::error_code error;
		boost::asio::write(
			socket,
			boost::asio::buffer(reinterpret_cast<std::uint8_t*>(this), sizeof(client::ClientAction)),
			error
		);
		return error;
	}

	void ClientAction::receiveAsync(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
									std::shared_ptr<ClientAction> clientAction,
									std::function<void(boost::system::error_code, std::shared_ptr<ClientAction>)> callback) {
		boost::asio::async_read(
			*socket,
			boost::asio::buffer(reinterpret_cast<uint8_t*>(clientAction.get()), sizeof(client::ClientAction)),
			[clientAction, callback](boost::system::error_code error, std::size_t) {
				callback(error, clientAction);
			}
		);
	}
}