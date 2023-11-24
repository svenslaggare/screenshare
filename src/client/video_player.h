#pragma once
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/array.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/button.h>
#include <gtkmm/image.h>
#include <gtkmm/box.h>
#include <gdkmm/pixbuf.h>
#include <glibmm/main.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/textview.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/scrolledwindow.h>

#include "info_text_buffer.h"
#include "actions.h"

#include "../misc/concurrency.hpp"

#include "../video/network.h"
#include "../video/decoder.h"

namespace screenshare::client {
	class VideoPlayer : public Gtk::Window {
	private:
		Gtk::Box mMainBox;
		Gtk::Box mControlPanelBox;

		Gtk::Button mConnectButton;
		Gtk::Button mDisconnectButton;

		InfoTextBuffer mInfoTextBuffer;
		std::uint64_t mInfoTextVersion = 0;
		Gtk::ScrolledWindow mInfoTextScroll;
		Gtk::TextView mInfoTextView;

		InfoTextBuffer mFrameInfoTextBuffer;
		std::uint64_t mFrameInfoTextVersion = 0;
		Gtk::TextView mFrameInfoTextView;

		Gtk::Image mImage;
		Glib::RefPtr<Gdk::Pixbuf> mPixBuf;
		Gtk::EventBox mImageEventBox;

		boost::asio::ip::tcp::endpoint mEndpoint;
		sigc::connection mTimerSlot;

		std::atomic<bool> mRun = false;
		std::thread mReceiveThread;

		misc::ResourceMutex<AVCodecParameters> mCodecParameters;
		misc::ResourceMutex<std::vector<client::ClientAction>> mClientActions;

		void addInfoLine(std::string line);

		void connectButtonClicked();
		void disconnectButtonClicked();
		bool keyPress(GdkEventKey* key);
		bool mouseButtonPress(GdkEventButton* mouseButton);

		bool onTimerCallback(int);

		void runFetchData();
		void fetchData();
	public:
		explicit VideoPlayer(boost::asio::ip::tcp::endpoint endpoint);
		virtual ~VideoPlayer() = default;
	};
}