#include "video_player.h"
#include "actions.h"

#include <gtkmm/cssprovider.h>

#include <fmt/format.h>

namespace screenshare::client {
	VideoPlayer::VideoPlayer(boost::asio::ip::tcp::endpoint endpoint)
		: mEndpoint(std::move(endpoint)),
		  mMainBox(Gtk::Orientation::ORIENTATION_VERTICAL),
		  mControlPanelBox(Gtk::Orientation::ORIENTATION_HORIZONTAL),
		  mConnectButton("Connect"),
		  mDisconnectButton("Disconnect"),
		  mInfoTextBuffer(30),
		  mFrameInfoTextBuffer(3),
		  mImage("assets/wait_for_connection.png"),
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

		mImage.set_valign(Gtk::Align::ALIGN_START);
		mImage.set_halign(Gtk::Align::ALIGN_START);

		mImageEventBox.add(mImage);
		mImageEventBox.set_size_request(1920, 1080);
		mImage.show();

		mMainBox.pack_start(mImageEventBox, false, false, 0);
		mImageEventBox.show();

		mMainBox.signal_key_press_event().connect(sigc::mem_fun(*this, &VideoPlayer::keyPress));
		mImageEventBox.signal_button_press_event().connect(sigc::mem_fun(*this, &VideoPlayer::mouseButtonPress));

		mMainBox.pack_start(mControlPanelBox, false, true, 0);
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

		mReceiveThread = std::jthread([&](std::stop_token stopToken) {
			runFetchData(stopToken);
		});
	}

	void VideoPlayer::fetchData(std::stop_token& stopToken) {
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

		addInfoLine(fmt::format(
			"Stream started {}x{} @ {} FPS",
			codecParameterReceiver.codecParameters()->width,
			codecParameterReceiver.codecParameters()->height,
			(double)codecParameterReceiver.timeBase().den / (double)codecParameterReceiver.timeBase().num
		));

		video::PacketDecoder packetDecoder;
		while (!stopToken.stop_requested()) {
			video::network::PacketHeader packetHeader;
			if (auto error = packetReceiver.receive(socket, packet.get(), packetHeader)) {
				if (error == boost::asio::error::eof) {
					addInfoLine("Connection closed by server.");
					break;
				} else if (error) {
					throw boost::system::system_error(error);
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

			auto response = packetDecoder.decode(
				packet.get(),
				packetReceiver.codecContext(),
				frame.get(),
				mPixBuf->get_pixels(),
				[&](AVCodecContext* codecContext) {
					std::timespec currentTime {};
					std::timespec_get(&currentTime, TIME_UTC);

					mFrameInfoTextBuffer.addLines({
						fmt::format("PTS: {} (delay: {})", frame->pts, packetHeader.encoderPts - frame->pts),
						fmt::format("Frame number: {}", frame->coded_picture_number),
						fmt::format("Time since packet sent {:.2f} ms", misc::elapsedSeconds(currentTime, packetHeader.sendTime) * 1000.0)
					});
				}
			);

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

	void VideoPlayer::runFetchData(std::stop_token& stopToken) {
		mIsConnected.store(true);

		try {
			fetchData(stopToken);
			addInfoLine("Stream ended.");
		} catch (const std::exception& e) {
			addInfoLine(fmt::format("Failed to connect due to: {}", e.what()));
		}

		mIsConnected.store(false);
	}

	bool VideoPlayer::onTimerCallback(int) {
		if (mPixBuf) {
			mImage.set(mPixBuf);
		}

		if (auto buffer = mInfoTextBuffer.gtkBufferIfUnchanged()) {
			mInfoTextView.set_buffer(buffer);

			auto adjustment = mInfoTextScroll.get_vadjustment();
			adjustment->set_value(adjustment->get_upper());
		}

		if (auto buffer = mFrameInfoTextBuffer.gtkBufferIfUnchanged()) {
			mFrameInfoTextView.set_buffer(buffer);
		}

		return true;
	}

	void VideoPlayer::addInfoLine(std::string line) {
		mInfoTextBuffer.addLine(std::move(line));
	}

	void VideoPlayer::disconnectButtonClicked() {
		mReceiveThread.request_stop();
	}

	void VideoPlayer::connectButtonClicked() {
		if (!mIsConnected.load()) {
			mReceiveThread = std::jthread([&](std::stop_token stopToken) {
				runFetchData(stopToken);
			});
		} else {
			addInfoLine("Already connected.");
		}
	}

	bool VideoPlayer::keyPress(GdkEventKey* key) {
		mClientActions.guard()->push_back(client::ClientAction::keyPressed(key->string));
		return false;
	}

	bool VideoPlayer::mouseButtonPress(GdkEventButton* mouseButton) {
		auto [width, height] = [&]() {
			auto codecParameters = mCodecParameters.guard();
			return std::make_tuple(codecParameters->width, codecParameters->height);
		}();

		mClientActions.guard()->push_back(client::ClientAction::mouseButtonPressed(
			mouseButton->button,
			mouseButton->x / width,
			mouseButton->y / height
		));

		return false;
	}
}