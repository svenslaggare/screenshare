#include "video_player.h"
#include "actions.h"

#include <fmt/format.h>

namespace screenshare::client {
	VideoPlayer::VideoPlayer(boost::asio::ip::tcp::endpoint endpoint)
		: mMainBox(Gtk::Orientation::ORIENTATION_VERTICAL),
		  mControlPanelBox(Gtk::Orientation::ORIENTATION_HORIZONTAL),
		  mConnectButton("Connect"),
		  mDisconnectButton("Disconnect"),
		  mImage("assets/wait_for_connection.png"),
		  mEndpoint(std::move(endpoint)),
		  mClientActions({}) {
		set_border_width(10);
		set_size_request(720, 480);

		add(mMainBox);
		mMainBox.show();

		mImageEventBox.add(mImage);
		mImage.show();

		mMainBox.pack_start(mImageEventBox, true, true, 0);
		mImageEventBox.show();

		mMainBox.signal_key_press_event().connect(sigc::mem_fun(*this, &VideoPlayer::keyPress));
		mImageEventBox.signal_button_press_event().connect(sigc::mem_fun(*this, &VideoPlayer::mouseButtonPress));

		mMainBox.pack_start(mControlPanelBox, true, true, 0);
		mControlPanelBox.show();

		mConnectButton.signal_clicked().connect(sigc::mem_fun(*this, &VideoPlayer::connectButtonClicked));
		mControlPanelBox.pack_start(mConnectButton, true, true, 0);
		mConnectButton.show();

		mDisconnectButton.signal_clicked().connect(sigc::mem_fun(*this, &VideoPlayer::disconnectButtonClicked));
		mControlPanelBox.pack_start(mDisconnectButton, true, true, 0);
		mDisconnectButton.show();

		mControlPanelBox.pack_start(mInfoTextView, true, true, 0);
		mInfoTextView.show();

		sigc::slot<bool ()> slot = sigc::bind(sigc::mem_fun(*this, &VideoPlayer::onTimerCallback), 0);
		mTimerSlot = Glib::signal_timeout().connect(slot, 16);

		mReceiveThread = std::thread([&]() {
			runFetchData();
		});
	}

	void VideoPlayer::fetchData() {
		boost::asio::io_context ioContext;
		boost::asio::ip::tcp::resolver resolver(ioContext);

		boost::asio::ip::tcp::socket socket(ioContext);
		socket.connect(mEndpoint);

		video::network::AVCodecParametersReceiver codecParameterReceiver(socket);
		video::network::PacketReceiver packetReceiver(codecParameterReceiver.codecParameters());

		std::unique_ptr<AVFrame, decltype([](auto* ptr) { av_frame_free(&ptr); })> frame(av_frame_alloc());
		if (!frame) {
			throw std::runtime_error("Failed to allocate memory for AVFrame");
		}

		std::unique_ptr<AVPacket, decltype([](auto* ptr) { av_packet_free(&ptr); })> packet(av_packet_alloc());
		if (!packet) {
			throw std::runtime_error("Failed to allocate memory for AVPacket");
		}

		addInfoLine(fmt::format("Stream started {}x{}", codecParameterReceiver.codecParameters()->width, codecParameterReceiver.codecParameters()->height));
		mRun.store(true);

		video::PacketDecoder packetDecoder;
		while (mRun.load()) {
			if (auto error = packetReceiver.receive(socket, packet.get())) {
				if (error == boost::asio::error::eof) {
					addInfoLine("Connection closed by server.");
					break; // Connection closed cleanly by peer.
				} else if (error) {
					throw boost::system::system_error(error); // Some other error.
				}
			}

			if (!mPixBuf) {
				mPixBuf = Gdk::Pixbuf::create(
					Gdk::Colorspace::COLORSPACE_RGB,
					false,
					8,
					codecParameterReceiver.codecParameters()->width,
					codecParameterReceiver.codecParameters()->height
				);
			}

//			misc::TimeMeasurement timeMeasurement("");
			auto response = packetDecoder.decode(
				packet.get(),
				packetReceiver.codecContext(),
				frame.get(),
				mPixBuf->get_pixels(),
				[&](AVCodecContext* codecContext) {}
			);
//			timeMeasurement.changePattern("decodePacket: " + std::to_string(response) + ", time: ");
//			timeMeasurement.print();

//			std::cout << std::endl;
			if (response < 0) {
				addInfoLine(fmt::format("Failed to decode packet ({})", response));
				break;
			}

			decltype(mClientActions)::Type clientActions;
			{
				auto guard = mClientActions.guard();
				clientActions = std::move(guard.get());
			}

			for (auto& clientAction : clientActions) {
				if (auto error = clientAction.send(socket)) {
					std::cout << "Failed to send action: " << error << std::endl;
					return;
				}
			}
		}
	}

	void VideoPlayer::runFetchData() {
		try {
			fetchData();
			addInfoLine("Stream ended.");
		} catch (const std::exception& e) {
			addInfoLine(fmt::format("Failed to connect due to: {}", e.what()));
		}

		mRun.store(false);
	}

	bool VideoPlayer::onTimerCallback(int) {
		if (mPixBuf) {
			mImage.set(mPixBuf);
		}

		mInfoTextView.set_buffer(mInfoBuffer.gtkBuffer());

		return true;
	}

	void VideoPlayer::addInfoLine(std::string line) {
		mInfoBuffer.addLine(std::move(line));
	}

	void VideoPlayer::disconnectButtonClicked() {
		mRun.store(false);
	}

	void VideoPlayer::connectButtonClicked() {
		if (!mRun.load()) {
			if (mReceiveThread.joinable()) {
				mReceiveThread.join();
			}

			mReceiveThread = std::thread([&]() {
				runFetchData();
			});
		}
	}

	bool VideoPlayer::keyPress(GdkEventKey* key) {
		std::cout << "Key press: " << key->string << std::endl;
		mClientActions.guard()->push_back(client::ClientAction::keyPressed(key->string));
		return false;
	}

	bool VideoPlayer::mouseButtonPress(GdkEventButton* mouseButton) {
//		mMainBox.grab_focus();
//		mImageEventBox.grab_focus();
		std::cout << "Mouse press" << std::endl;
		return false;
	}
}