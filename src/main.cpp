#include <string>

#include "client/video_player.h"
#include "screengrabber/x11.h"

#include "server/video_server.h"

#include "misc/network.h"

using namespace screenshare;

void mainServer(const std::string& bind) {
	server::VideoServer videoServer(misc::tcpEndpointFromString(bind));
	videoServer.run(":0", 0x1dd);
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

	if ((argc >= 3) && std::string(argv[1]) == "server") {
		mainServer(argv[2]);
		return 0;
	}

	return 1;
}