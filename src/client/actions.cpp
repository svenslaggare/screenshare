#include "actions.h"

namespace screenshare::client {
	KeyPressedClientAction KeyPressedClientAction::fromString(const std::string& keyString) {
		KeyPressedClientAction action;
		for (std::size_t i = 0; i < std::min<std::size_t>(keyString.size(), 4); i++) {
			action.key[i] = keyString[i];
		}

		return action;
	}

	ClientAction ClientAction::keyPressed(const std::string& key) {
		return ClientAction {
			.type = client::ClientActionType::KeyPressed,
			.data = { client::KeyPressedClientAction::fromString(key) }
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
									std::function<void(boost::system::error_code, std::shared_ptr<ClientAction>)> callback) {
		auto clientAction = std::make_shared<client::ClientAction>();

		boost::asio::async_read(
			*socket,
			boost::asio::buffer(reinterpret_cast<uint8_t*>(clientAction.get()), sizeof(client::ClientAction)),
			[clientAction, callback](boost::system::error_code error, std::size_t) {
				callback(error, clientAction);
			}
		);
	}
}