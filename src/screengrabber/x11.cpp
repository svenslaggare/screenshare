#include "x11.h"

namespace screenshare::screengrabber {
	ScreenGrabberX11::ScreenGrabberX11(const GrabberSpec& spec)
		: mDisplay(XOpenDisplay(spec.displayName.c_str())),
		  mWindowId(spec.windowId),
		  mX11SharedMemory({}) {
		XWindowAttributes attributes;
		XGetWindowAttributes(mDisplay, spec.windowId, &attributes);
		mWidth = attributes.width;
		mHeight = attributes.height;

		auto screen = DefaultScreen(mDisplay);
		mX11SharedMemory.shmseg = 0;
		mX11SharedMemory.shmid = -1;
		mX11SharedMemory.shmaddr = (char*)-1;
		mX11SharedMemory.readOnly = false;

		mImage = XShmCreateImage(
			mDisplay,
			DefaultVisual(mDisplay, screen),
			DefaultDepth(mDisplay, screen),
			ZPixmap,
			nullptr,
			&mX11SharedMemory,
			this->mWidth,
			this->mHeight
		);

		mX11SharedMemory.shmid = shmget(IPC_PRIVATE, mImage->bytes_per_line * mImage->height, IPC_CREAT | 0700);
		mX11SharedMemory.shmaddr = (char*)shmat(mX11SharedMemory.shmid, nullptr, SHM_RND);
		mImage->data = mX11SharedMemory.shmaddr;
		XShmAttach(mDisplay, &mX11SharedMemory);
	}

	ScreenGrabberX11::~ScreenGrabberX11() {
		XShmDetach(mDisplay, &mX11SharedMemory);
		XDestroyImage(mImage);
	}

	int ScreenGrabberX11::width() const {
		return mWidth;
	}

	int ScreenGrabberX11::height() const {
		return mHeight;
	}

	GrabbedFrame ScreenGrabberX11::grab() {
		XShmGetImage(mDisplay, mWindowId, mImage, 0, 0, AllPlanes);
		return {
			mImage->width,
			mImage->height,
			AVPixelFormat::AV_PIX_FMT_BGRA,
			(std::uint8_t*)mImage->data,
			mImage->width * 4
		};
	}
}