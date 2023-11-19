#pragma once

#include <deque>
#include <string>

#include <glibmm/refptr.h>
#include <gtkmm/textbuffer.h>

namespace screenshare::client {
	class InfoBuffer {
	private:
		std::size_t mMaxLines = 3;
		std::deque<std::string> mLines;
		std::mutex mMutex;

		Glib::RefPtr<Gtk::TextBuffer> mGtkBuffer;
	public:
		InfoBuffer();

		Glib::RefPtr<Gtk::TextBuffer> gtkBuffer();

		void addLine(std::string newLine);
	};
}