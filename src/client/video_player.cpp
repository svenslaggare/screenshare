#include "video_player.h"
#include "actions.h"

#include <gtkmm/cssprovider.h>

#include <fmt/format.h>

namespace screenshare::client {
	VideoPlayer::VideoPlayer(boost::asio::ip::tcp::endpoint endpoint)
		: mMainBox(Gtk::Orientation::ORIENTATION_VERTICAL),
		  mControlPanelBox(Gtk::Orientation::ORIENTATION_HORIZONTAL),
		  mConnectButton("Connect"),
		  mDisconnectButton("Disconnect"),
		  mInfoTextBuffer(30),
		  mFrameInfoTextBuffer(3),
		  mImage("assets/wait_for_connection.png"),
		  mEndpoint(std::move(endpoint)),
		  mCodecParameters({}),
		  mClientActions({}) {
		set_border_width(10);

		auto css = Gtk::CssProvider::create();
		css->load_from_path("assets/main.css");
		get_style_context()->add_provider_for_screen(
			Gdk::Screen::get_default(),
			css,
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
		);

		constexpr int MARGIN = 10;

		add(mMainBox);
		mMainBox.show();

		mImageEventBox.add(mImage);
		mImage.show();

		mMainBox.pack_start(mImageEventBox, true, true, 0);
		mImageEventBox.show();

		mMainBox.signal_key_press_event().connect(sigc::mem_fun(*this, &VideoPlayer::keyPress));
		mImageEventBox.signal_button_press_event().connect(sigc::mem_fun(*this, &VideoPlayer::mouseButtonPress));

		mMainBox.pack_start(mControlPanelBox, true, true, 0);
		mControlPanelBox.set_margin_top(MARGIN);
		mControlPanelBox.show();

		mConnectButton.signal_clicked().connect(sigc::mem_fun(*this, &VideoPlayer::connectButtonClicked));
		mControlPanelBox.pack_start(mConnectButton, false, false, 0);
		mConnectButton.set_margin_left(MARGIN);
		mConnectButton.show();

		mDisconnectButton.signal_clicked().connect(sigc::mem_fun(*this, &VideoPlayer::disconnectButtonClicked));
		mControlPanelBox.pack_start(mDisconnectButton, false, false, 0);
		mDisconnectButton.set_margin_left(MARGIN);
		mDisconnectButton.show();

		mInfoTextScroll.add(mInfoTextView);
		mInfoTextView.show();

		mInfoTextScroll.set_size_request(700, 50);
		mControlPanelBox.pack_start(mInfoTextScroll, false, false, 0);
		mInfoTextScroll.set_margin_left(MARGIN);
		mInfoTextScroll.show();

		mControlPanelBox.pack_start(mFrameInfoTextView, false, false, 0);
		mFrameInfoTextView.set_size_request(350, 50);
		mFrameInfoTextView.set_margin_left(MARGIN);
		mFrameInfoTextView.show();

		sigc::slot<bool ()> slot = sigc::bind(sigc::mem_fun(*this, &VideoPlayer::onTimerCallback), 0);
		mTimerSlot = Glib::signal_timeout().connect(slot, 16);

		mReceiveThread = std::thread([&]() {
			runFetchData();
		});
	}

	VideoPlayer::~VideoPlayer() {
		mRun.store(false);
		if (mReceiveThread.joinable()) {
			mReceiveThread.join();
		}
	}

	void VideoPlayer::fetchData() {
		boost::asio::io_context ioContext;
		boost::asio::ip::tcp::resolver resolver(ioContext);

		boost::asio::ip::tcp::socket socket(ioContext);
		socket.connect(mEndpoint);

		video::network::AVCodecParametersReceiver codecParameterReceiver(socket);
		mCodecParameters.guard().get() = *codecParameterReceiver.codecParameters();
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
				[&](AVCodecContext* codecContext) {
					mFrameInfoTextBuffer.addLines({
						fmt::format("DTS: {}", frame->pkt_dts),
						fmt::format("PTS: {}", frame->coded_picture_number),
						fmt::format("Packet size: {}", packet->size)
					});
				}
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
				clientActions = std::move(mClientActions.guard().get());
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

		{
			std::uint64_t nextVersion = 0;
			auto buffer = mInfoTextBuffer.gtkBuffer(&nextVersion);
			if (nextVersion != mInfoTextVersion) {
				mInfoTextVersion = nextVersion;
				mInfoTextView.set_buffer(buffer);

				auto adjustment = mInfoTextScroll.get_vadjustment();
				adjustment->set_value(adjustment->get_upper());
			}
		}

		{
			std::uint64_t nextVersion = 0;
			auto buffer = mFrameInfoTextBuffer.gtkBuffer(&nextVersion);
			if (nextVersion != mFrameInfoTextVersion) {
				mFrameInfoTextVersion = nextVersion;
				mFrameInfoTextView.set_buffer(buffer);
			}
		}
		return true;
	}

	void VideoPlayer::addInfoLine(std::string line) {
		mInfoTextBuffer.addLine(std::move(line));
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
		mClientActions.guard()->push_back(client::ClientAction::keyPressed(key->string));
		return false;
	}

	bool VideoPlayer::mouseButtonPress(GdkEventButton* mouseButton) {
//		mMainBox.grab_focus();
//		mImageEventBox.grab_focus();

		double width;
		double height;
		{
			auto codecParameters = mCodecParameters.guard();
			width = codecParameters->width;
			height = codecParameters->height;
		}

		mClientActions.guard()->push_back(client::ClientAction::mouseButtonPressed(
			mouseButton->button,
			mouseButton->x / width,
			mouseButton->y / height
		));

		return false;
	}
}