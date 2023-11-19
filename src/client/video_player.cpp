#include "video_player.h"

namespace screenshare::client {
	VideoPlayer::VideoPlayer(boost::asio::ip::tcp::endpoint endpoint)
		: mMainBox(Gtk::Orientation::ORIENTATION_VERTICAL),
		  mControlPanelBox(Gtk::Orientation::ORIENTATION_HORIZONTAL),
		  mConnectButton("Connect"),
		  mDisconnectButton("Disconnect"),
		  mEndpoint(std::move(endpoint)) {
		set_border_width(10);
		set_size_request(720, 480);

		add(mMainBox);
		mMainBox.show();

		mImage.set(mPixBuf);
		mMainBox.pack_start(mImage, true, true, 0);
		mImage.show();

		mMainBox.pack_start(mControlPanelBox, true, true, 0);
		mControlPanelBox.show();

		mConnectButton.signal_clicked().connect(sigc::mem_fun(*this, &VideoPlayer::connectButtonClicked));
		mControlPanelBox.pack_start(mConnectButton, true, true, 0);
		mConnectButton.show();

		mDisconnectButton.signal_clicked().connect(sigc::mem_fun(*this, &VideoPlayer::disconnectButtonClicked));
		mControlPanelBox.pack_start(mDisconnectButton, true, true, 0);
		mDisconnectButton.show();

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

		std::cout << "Stream started " << codecParameterReceiver.codecParameters()->width << "x" << codecParameterReceiver.codecParameters()->height << std::endl;
		mRun.store(true);

		video::PacketDecoder packetDecoder;
		while (mRun.load()) {
			if (auto error = packetReceiver.receive(socket, packet.get())) {
				if (error == boost::asio::error::eof) {
					std::cout << "Connected closed by server." << std::endl;
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

			misc::TimeMeasurement timeMeasurement("");
			auto response = packetDecoder.decode(
				packet.get(),
				packetReceiver.codecContext(),
				frame.get(),
				mPixBuf->get_pixels(),
				[&](AVCodecContext* codecContext) {

				}
			);
			timeMeasurement.changePattern("decodePacket: " + std::to_string(response) + ", time: ");
			timeMeasurement.print();

			std::cout << std::endl;
			if (response < 0) {
				std::cout << "Failed to decode packet (" << response << ")." << std::endl;
				break;
			}
		}
	}

	void VideoPlayer::runFetchData() {
		try {
			fetchData();
			std::cout << "Done with fetch data." << std::endl;
		} catch (const std::exception& e) {
			std::cout << "Failed to connect due to: " << e.what() << std::endl;
		}
	}

	bool VideoPlayer::onTimerCallback(int) {
		mImage.set(mPixBuf);
		return true;
	}

	void VideoPlayer::disconnectButtonClicked() {
		mRun.store(false);
	}

	void VideoPlayer::connectButtonClicked() {
		if (mReceiveThread.joinable()) {
			mReceiveThread.join();
		}

		mReceiveThread = std::thread([&]() {
			runFetchData();
		});
	}
}