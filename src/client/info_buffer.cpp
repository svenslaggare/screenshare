#include "info_buffer.h"

namespace screenshare::client {
	InfoBuffer::InfoBuffer()
		: mGtkBuffer(Gtk::TextBuffer::create()) {

	}

	Glib::RefPtr<Gtk::TextBuffer> InfoBuffer::gtkBuffer() const {
		return mGtkBuffer;
	}

	void InfoBuffer::addLine(std::string newLine) {
		if (mLines.size() > 1) {
			mLines.pop_front();
		}

		mLines.push_back(std::move(newLine));

		std::stringstream stream;
		for (auto& line : mLines) {
			stream << line << std::endl;
		}

		mGtkBuffer->set_text(stream.str());
	}
}