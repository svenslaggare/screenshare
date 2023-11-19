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
		video::VideoEncoder videoEncoder("mp4");
		auto videoStream = videoEncoder.addVideoStream(1920, 1080, 30);
		if (!videoStream) {
			return;
		}

		mRun.store(true);

		accept(videoStream);

		auto contextThread = std::thread([&]() {
			boost::system::error_code error;

			while (!error.failed() && mRun.load()) {
				mIOContext.run(error);
			}

			std::cout << "Context done with error: " << error << std::endl;
		});

		screengrabber::ScreenGrabberX11 screenGrabber(displayName, windowId);
		std::cout << "Grabbing: " << screenGrabber.width() << "x" << screenGrabber.height() << std::endl;

		auto streamFrameRate = (double)videoEncoder.streams()[0].encoder->time_base.den
								/ (double)videoEncoder.streams()[0].encoder->time_base.num;

		video::Converter converter;
		video::network::PacketSender packetSender;
		while (true) {
			misc::RateSleeper rateSleeper(streamFrameRate);
//			misc::TimeMeasurement frameTM("Frame time");

//			std::cout << "Frame PTS: " << videoStream->frame->pts << std::endl;
//			misc::TimeMeasurement grabTM("Grab time");
			auto grabbedFrame = screenGrabber.grab();
//			grabTM.print();

			if (!createFrame(videoStream, converter, grabbedFrame)) {
				break;
			}

			std::vector<std::tuple<ClientId, Socket*>> clientSockets;
			{
				std::lock_guard<std::mutex> guard(mClientSocketsMutex);
				for (auto& [clientId, socket] : mClientSockets) {
					clientSockets.emplace_back( clientId, socket.get() );
				}
			}

			auto [done, socketErrors] = encodeFrameAndSend(clientSockets, videoStream, packetSender);

			{
				std::lock_guard<std::mutex> guard(mClientSocketsMutex);
				for (auto& [clientId, socketError] : socketErrors) {
					if (socketError) {
						std::cout << "Removing client #" << clientId << " due to: " << socketError << std::endl;
						mClientSockets.erase(clientId);
					}
				}
			}

//			frameTM.print();
//			std::cout << std::endl;

			if (done) {
				break;
			}
		}

		std::cout << "Done encoding." << std::endl;

		mRun.store(false);
		mIOContext.stop();

		if (contextThread.joinable()) {
			contextThread.join();
		}
	}

	void VideoServer::accept(video::OutputStream* videoStream) {
		auto socket = std::make_shared<Socket>(mIOContext);

		mAcceptor.async_accept(
			*socket,
			[this, videoStream, socket](boost::system::error_code acceptFailed) {
				if (!acceptFailed) {
					if (auto error = screenshare::video::network::sendAVCodecParameters(*socket, videoStream->encoder.get())) {
						std::cout << "Failed to send codec parameters due to: " << error << std::endl;
					} else {
						std::lock_guard<std::mutex> guard(mClientSocketsMutex);
						auto clientId = mNextClientId++;
						std::cout << "Accepted client #" << clientId << ": " << socket->remote_endpoint() << std::endl;
						mClientSockets[clientId] = socket;
					}
				} else {
					std::cout << "Failed to accept client due to: " << acceptFailed << std::endl;
				}

				accept(videoStream);
			}
		);
	}

	bool VideoServer::createFrame(video::OutputStream* videoStream,
								  video::Converter& converter,
								  const screengrabber::GrabbedFrame& grabbedFrame) {
		if (av_frame_make_writable(videoStream->frame.get()) < 0) {
			std::cout << "av_frame_make_writable failed" << std::endl;
			return false;
		}

//		misc::TimeMeasurement timeMeasurement("Convert time");
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

	std::tuple<bool, std::vector<VideoServer::SendResult>> VideoServer::encodeFrameAndSend(std::vector<std::tuple<ClientId, Socket*>>& sockets,
																						   video::OutputStream* videoStream,
																						   video::network::PacketSender& packetSender) {
		if (avcodec_send_frame(videoStream->encoder.get(), videoStream->frame.get()) < 0) {
			std::cout << "avcodec_send_frame failed" << std::endl;
			return { true, {} };
		}


		bool done = false;
		auto packet = videoStream->packet.get();
		auto stream = videoStream->stream;

		std::vector<SendResult> socketErrors;
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

			std::vector<std::tuple<ClientId, std::shared_ptr<video::network::PacketSender::AsyncResult>>> sendResults;
			for (auto& [clientId, socket] : sockets) {
				sendResults.emplace_back(clientId, packetSender.sendAsync(*socket, packet));
			}

			for (auto [clientId, sendResult] : sendResults) {
				sendResult->done.wait(false);

				if (sendResult->error) {
					socketErrors.emplace_back(clientId, sendResult->error);
				}
			}
		}

		return { done, socketErrors };
	}
}