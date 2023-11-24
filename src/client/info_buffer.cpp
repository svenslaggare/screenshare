#include "info_buffer.h"

namespace screenshare::client {
	InfoBuffer::InfoBuffer()
		: mGtkBuffer(Gtk::TextBuffer::create()) {

	}

	std::size_t InfoBuffer::maxLines() const {
		return mMaxLines;
	}

	Glib::RefPtr<Gtk::TextBuffer> InfoBuffer::gtkBuffer() {
		std::lock_guard<std::mutex> guard(mMutex);

		std::stringstream stream;
		for (auto& line : mLines) {
			stream << line << std::endl;
		}

		mGtkBuffer->set_text(stream.str());

		return mGtkBuffer;
	}

	void InfoBuffer::addLine(std::string newLine) {
		std::lock_guard<std::mutex> guard(mMutex);

		if (mLines.size() >= mMaxLines) {
			mLines.pop_front();
		}

		mLines.push_back(std::move(newLine));
	}
}