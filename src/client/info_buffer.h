#pragma once

#include <deque>
#include <string>

#include <glibmm/refptr.h>
#include <gtkmm/textbuffer.h>

namespace screenshare::client {
	class InfoBuffer {
	private:
		std::deque<std::string> mLines;
		Glib::RefPtr<Gtk::TextBuffer> mGtkBuffer;
	public:
		InfoBuffer();

		Glib::RefPtr<Gtk::TextBuffer> gtkBuffer() const;

		void addLine(std::string newLine);
	};
}