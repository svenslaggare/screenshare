#pragma once

#include <string>
#include <tuple>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include "../video/common.h"

namespace screenshare::screengrabber {
	struct GrabbedFrame {
		int width = 0;
		int height = 0;
		AVPixelFormat format = AVPixelFormat::AV_PIX_FMT_NONE;
		std::uint8_t* data = nullptr;
		int lineSize = 0;
	};

	class ScreenGrabberX11 {
	private:
		Display* mDisplay = nullptr;
		int mWindowId;
		int mWidth;
		int mHeight;

		XShmSegmentInfo mX11SharedMemory;
		XImage* mImage = nullptr;
	public:
		ScreenGrabberX11(const std::string& displayName, int windowId);
		~ScreenGrabberX11();

		int width() const;
		int height() const;

		GrabbedFrame grab();
	};
}