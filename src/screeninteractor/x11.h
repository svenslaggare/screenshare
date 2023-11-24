#pragma once

#include <string>
#include <tuple>

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "common.h"

#include "../video/common.h"
#include "../client/actions.h"

namespace screenshare::screeninteractor {
	class ScreenInteractorX11 : public ScreenInteractor {
	private:
		Display* mDisplay = nullptr;
		int mWindowId;
		int mWidth;
		int mHeight;

		XShmSegmentInfo mX11SharedMemory;
		XImage* mImage = nullptr;

		void makeWindowActive();
	public:
		struct GrabberSpec {
			std::string displayName;
			int windowId = 0;
		};

		explicit ScreenInteractorX11(const GrabberSpec& spec);
		~ScreenInteractorX11() override;

		int width() const override;
		int height() const override;

		GrabbedFrame grab() override;
		virtual bool handleClientAction(const client::ClientAction& clientAction) override;
	};
}