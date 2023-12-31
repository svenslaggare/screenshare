#include <string>

#include "misc/network.h"

#include "client/video_player.h"

#include "screeninteractor/x11.h"
#include "server/video_server.h"

using namespace screenshare;

void mainServer(const std::string& bind, int windowId) {
	server::VideoServer videoServer(misc::tcpEndpointFromString(bind), { 1920, 1080, 30 });
	videoServer.run(std::unique_ptr<screeninteractor::ScreenInteractor>(new screeninteractor::ScreenInteractorX11({ ":0", windowId })));
}

int mainClient(const std::string& endpoint) {
	std::string programName = "screenshare";
	std::vector<char*> programArguments { (char*)programName.c_str() };
	auto numProgramArguments = (int)programArguments.size();
	auto programArgumentsPtr = programArguments.data();

	auto app = Gtk::Application::create(
		numProgramArguments,
		programArgumentsPtr,
		"com.screenshare",
		Gio::ApplicationFlags::APPLICATION_NON_UNIQUE
	);
	client::VideoPlayer videoPlayer(misc::tcpEndpointFromString(endpoint));
	return app->run(videoPlayer);
}

int main(int argc, char* argv[]) {
	if ((argc >= 3) && std::string(argv[1]) == "client") {
		return mainClient(argv[2]);
	}

	if ((argc >= 4) && std::string(argv[1]) == "server") {
		mainServer(argv[2], std::stoi(argv[3]));
		return 0;
	}

	return 1;
}