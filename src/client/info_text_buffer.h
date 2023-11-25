#pragma once

#include <deque>
#include <string>

#include <glibmm/refptr.h>
#include <gtkmm/textbuffer.h>

namespace screenshare::client {
	class InfoTextBuffer {
	private:
		std::size_t mMaxLines;

		std::deque<std::string> mLines;
		std::uint64_t mVersion = 1;
		std::mutex mMutex;

		Glib::RefPtr<Gtk::TextBuffer> mGtkBuffer;
		std::uint64_t mGtkBufferVersion = 0;
	public:
		explicit InfoTextBuffer(std::size_t maxLines);

		std::size_t maxLines() const;

		Glib::RefPtr<Gtk::TextBuffer> gtkBuffer(bool& changed);

		void addLine(std::string newLine);
		void addLines(std::vector<std::string> newLines);
	};
}