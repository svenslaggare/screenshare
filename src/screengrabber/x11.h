#pragma once

#include <string>
#include <tuple>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include "../video/common.h"
#include "common.h"

namespace screenshare::screengrabber {
	class ScreenGrabberX11 : public ScreenGrabber {
	private:
		Display* mDisplay = nullptr;
		int mWindowId;
		int mWidth;
		int mHeight;

		XShmSegmentInfo mX11SharedMemory;
		XImage* mImage = nullptr;
	public:
		struct GrabberSpec {
			std::string displayName;
			int windowId = 0;
		};

		explicit ScreenGrabberX11(const GrabberSpec& spec);
		~ScreenGrabberX11() override;

		int width() const override;
		int height() const override;

		GrabbedFrame grab() override;
	};
}