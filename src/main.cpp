#include <string>

#include <boost/array.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

#include "video/encoder.h"
#include "video/decoder.h"
#include "video/network.h"

#include "client/video_player.h"

#include "screengrabber/x11.h"

#include "misc/rate_sleeper.h"
#include "misc/network.h"

using boost::asio::ip::tcp;
using namespace screenshare;

bool createFrame(video::OutputStream* videoStream,
				 video::Converter& converter,
				 const screengrabber::GrabbedFrame& grabbedFrame) {
	if (av_frame_make_writable(videoStream->frame.get()) < 0) {
		std::cout << "av_frame_make_writable failed" << std::endl;
		return false;
	}

	misc::TimeMeasurement timeMeasurement("Convert time");
	auto convertResult = converter.convert(
		grabbedFrame.width, grabbedFrame.height, grabbedFrame.format,
		grabbedFrame.data, grabbedFrame.lineSize,

		videoStream->encoder->width, videoStream->encoder->height, videoStream->encoder->pix_fmt,
		videoStream->frame->data, videoStream->frame->linesize
	);

	if (!convertResult) {
		std::cout << "convert failed" << std::endl;
		return false;
	}

	videoStream->frame->pts = videoStream->nextPts++;
	return true;
}

bool encodeFrameAndSend(tcp::socket& socket,
						video::OutputStream* videoStream,
						video::network::PacketSender& packetSender) {
	if (avcodec_send_frame(videoStream->encoder.get(), videoStream->frame.get()) < 0) {
		std::cout << "avcodec_send_frame failed" << std::endl;
		return true;
	}

	bool done = false;
	auto packet = videoStream->packet.get();
	auto stream = videoStream->stream;
	while (true) {
		auto response = avcodec_receive_packet(videoStream->encoder.get(), packet);
		if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
			done = response == AVERROR_EOF;
			break;
		} else if (response < 0) {
			std::cout << "avcodec_receive_packet failed" << std::endl;
			break;
		}

		//rescale output packet timestamp values from codec to stream timebase
		av_packet_rescale_ts(packet, videoStream->encoder->time_base, stream->time_base);
		packet->stream_index = stream->index;

		misc::TimeMeasurement sendTM("Send time");
		if (auto error = packetSender.send(socket, packet)) {
			std::cout << error << std::endl;
			done = true;
			break;
		}
	}

	return done;
}

void mainServer(const std::string& bind) {
	boost::asio::io_context ioContext;
	tcp::acceptor acceptor(ioContext, misc::tcpEndpointFromString(bind));
	std::cout << "Running at " << bind << std::endl;

	std::string displayName = ":0";
	int windowId = 0x1dd;
	screengrabber::ScreenGrabberX11 screenGrabber(displayName, windowId);
	std::cout << "Grabbing: " << screenGrabber.width() << "x" << screenGrabber.height() << std::endl;

	while (true) {
		tcp::socket socket(ioContext);
		acceptor.accept(socket);
		std::cout << "Accepted: " << socket.remote_endpoint() << std::endl;

		video::VideoEncoder videoEncoder("mp4");
		auto videoStream = videoEncoder.addVideoStream(1920, 1080, 30);
		if (!videoStream) {
			continue;
		}

		if (auto error = screenshare::video::network::sendAVCodecParameters(socket, videoStream->encoder.get())) {
			std::cout << error << std::endl;
			continue;
		}

		auto streamFrameRate = (double)videoEncoder.streams()[0].encoder->time_base.den / (double)videoEncoder.streams()[0].encoder->time_base.num;

		video::Converter converter;
		video::network::PacketSender packetSender;
		while (true) {
			misc::RateSleeper rateSleeper(streamFrameRate);
			misc::TimeMeasurement frameTM("Frame time");

			std::cout << "Frame PTS: " << videoStream->frame->pts << std::endl;
			misc::TimeMeasurement grabTM("Grab time");
			auto grabbedFrame = screenGrabber.grab();
			grabTM.print();

			if (!createFrame(videoStream, converter, grabbedFrame)) {
				break;
			}

			auto done = encodeFrameAndSend(socket, videoStream, packetSender);

			frameTM.print();
			std::cout << std::endl;

			if (done) {
				break;
			}
		}

		std::cout << "Done encoding." << std::endl;
	}
}

int mainClient(const std::string& endpoint) {
	std::string programName = "screenshare";
	std::vector<char*> programArguments { (char*)programName.c_str() };
	auto numProgramArguments = (int)programArguments.size();
	auto programArgumentsPtr = programArguments.data();

	auto app = Gtk::Application::create(numProgramArguments, programArgumentsPtr, "com.screenshare");
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