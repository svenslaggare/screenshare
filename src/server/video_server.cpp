#include <iostream>

#include "video_server.h"
#include "../screengrabber/x11.h"
#include "../video/encoder.h"
#include "../video/network.h"
#include "../misc/rate_sleeper.h"

namespace screenshare::server {
	VideoServer::VideoServer(boost::asio::ip::tcp::endpoint bind)
		: mAcceptor(mIOContext, bind) {
		std::cout << "Running at " << bind << std::endl;
	}

	void VideoServer::run(const std::string& displayName, int windowId) {
		screengrabber::ScreenGrabberX11 screenGrabber(displayName, windowId);
		std::cout << "Grabbing: " << screenGrabber.width() << "x" << screenGrabber.height() << std::endl;

		while (true) {
			boost::asio::ip::tcp::socket socket(mIOContext);
			mAcceptor.accept(socket);
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

	bool VideoServer::createFrame(video::OutputStream* videoStream,
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

	bool VideoServer::encodeFrameAndSend(boost::asio::ip::tcp::socket& socket,
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
}