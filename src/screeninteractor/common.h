#pragma once
#include <optional>

#include "../video/common.h"

namespace screenshare::client {
	struct ClientAction;
}

namespace screenshare::screeninteractor {
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

		virtual std::optional<GrabbedFrame> grab() = 0;
	};

	class ClientActionHandler {
	public:
		virtual ~ClientActionHandler() = default;
		virtual bool handleClientAction(const client::ClientAction& clientAction) = 0;
	};

	class ScreenInteractor : public ScreenGrabber, public ClientActionHandler {

	};
}