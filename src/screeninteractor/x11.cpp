#include "x11.h"

#include <iostream>
#include <thread>

#include <X11/extensions/XTest.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>

namespace screenshare::screeninteractor {
	ScreenInteractorX11::ScreenInteractorX11(const GrabberSpec& spec)
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

		XSetErrorHandler([](Display * d, XErrorEvent * e) {
			std::cerr << "Error code: " << e->error_code << std::endl;
			return 0;
		});
	}

	ScreenInteractorX11::~ScreenInteractorX11() {
		XShmDetach(mDisplay, &mX11SharedMemory);
		XDestroyImage(mImage);
		XCloseDisplay(mDisplay);
	}

	int ScreenInteractorX11::width() const {
		return mWidth;
	}

	int ScreenInteractorX11::height() const {
		return mHeight;
	}

	GrabbedFrame ScreenInteractorX11::grab() {
		XShmGetImage(mDisplay, mWindowId, mImage, 0, 0, AllPlanes);
		return {
			mImage->width,
			mImage->height,
			AVPixelFormat::AV_PIX_FMT_BGRA,
			(std::uint8_t*)mImage->data,
			mImage->width * 4
		};
	}

	bool ScreenInteractorX11::handleClientAction(const client::ClientAction& clientAction) {
		switch (clientAction.type) {
			case client::ClientActionType::NoAction:
				break;
			case client::ClientActionType::KeyPressed: {
				std::string key { clientAction.data.keyPress.key };
				auto keyCode = XKeysymToKeycode(mDisplay, XStringToKeysym(key.c_str()));

				XEvent event {};
				event.type = ClientMessage;
				event.xclient.display = mDisplay;
				event.xclient.window = mWindowId;
				event.xclient.message_type = XInternAtom(mDisplay, "_NET_ACTIVE_WINDOW", False);
				event.xclient.format = 32;
				event.xclient.data.l[0] = 2L; /* 2 == Message from a window pager */
				event.xclient.data.l[1] = CurrentTime;

				XWindowAttributes windowAttributes {};
				XGetWindowAttributes(mDisplay, mWindowId, &windowAttributes);
				XSendEvent(
					mDisplay,
					windowAttributes.screen->root,
					False,
					SubstructureNotifyMask | SubstructureRedirectMask,
					&event
				);
				XFlush(mDisplay);

				std::this_thread::sleep_for(std::chrono::milliseconds(200));

				XTestFakeKeyEvent(mDisplay, keyCode, True, 0);
				XFlush(mDisplay);

				XTestFakeKeyEvent(mDisplay, keyCode, False, 0);
				XFlush(mDisplay);
				break;
			}
		}

		return false;
	}
}