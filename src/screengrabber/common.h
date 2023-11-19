#pragma once
#include "../video/common.h"

namespace screenshare::screengrabber {
	struct GrabbedFrame {
		int width = 0;
		int height = 0;
		AVPixelFormat format = AVPixelFormat::AV_PIX_FMT_NONE;
		std::uint8_t* data = nullptr;
		int lineSize = 0;
	};

	class ScreenGrabber {
	public:
		virtual ~ScreenGrabber() = default;

		virtual int width() const = 0;
		virtual int height() const = 0;

		virtual GrabbedFrame grab() = 0;
	};
}