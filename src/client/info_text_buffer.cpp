#include "info_text_buffer.h"

namespace screenshare::client {
	InfoTextBuffer::InfoTextBuffer(std::size_t maxLines)
		: mMaxLines(maxLines), mGtkBuffer(Gtk::TextBuffer::create()) {

	}

	std::size_t InfoTextBuffer::maxLines() const {
		return mMaxLines;
	}

	Glib::RefPtr<Gtk::TextBuffer> InfoTextBuffer::gtkBuffer(bool& changed) {
		std::lock_guard<std::mutex> guard(mMutex);

		if (mGtkBufferVersion != mVersion) {
			std::stringstream stream;
			bool isFirst = true;
			for (auto& line: mLines) {
				if (!isFirst) {
					stream << std::endl;
				} else {
					isFirst = false;
				}

				stream << line;
			}

			mGtkBuffer->set_text(stream.str());
			mGtkBufferVersion = mVersion;

			changed = true;
		} else {
			changed = false;
		}

		return mGtkBuffer;
	}

	Glib::RefPtr<Gtk::TextBuffer> InfoTextBuffer::gtkBufferIfUnchanged() {
		bool changed = false;
		auto buffer = gtkBuffer(changed);
		if (changed) {
			return buffer;
		} else {
			return {};
		}
	}

	void InfoTextBuffer::addLine(std::string newLine) {
		std::lock_guard<std::mutex> guard(mMutex);

		if (mLines.size() >= mMaxLines) {
			mLines.pop_front();
		}

		mLines.push_back(std::move(newLine));
		mVersion++;
	}

	void InfoTextBuffer::addLines(std::vector<std::string> newLines) {
		std::lock_guard<std::mutex> guard(mMutex);

		for (auto&& line : newLines) {
			if (mLines.size() >= mMaxLines) {
				mLines.pop_front();
			}

			mLines.push_back(std::move(line));
		}

		mVersion++;
	}
}