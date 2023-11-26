#include <iostream>

#include "video_server.h"
#include "../video/encoder.h"
#include "../video/network.h"
#include "../misc/rate_sleeper.h"

namespace screenshare::server {
	VideoServer::VideoServer(boost::asio::ip::tcp::endpoint bind, video::VideoEncoderConfig videoEncoderConfig)
		: mVideoEncoderConfig(videoEncoderConfig),
		  mAcceptor(mIOContext, bind),
		  mClientSockets({}),
		  mClientActions({}) {
		std::cout << "Running at " << bind << std::endl;
	}

	void VideoServer::run(std::unique_ptr<screeninteractor::ScreenInteractor> screenInteractor) {
		video::VideoEncoder videoEncoder("mp4");
		auto videoStream = videoEncoder.addVideoStream(mVideoEncoderConfig);
		if (!videoStream) {
			throw std::runtime_error("Failed to create video stream.");
		}

		accept(videoStream);

		mIOContextThread = std::jthread([&](std::stop_token stopToken) {
			boost::system::error_code error;

			while (!error.failed() && !stopToken.stop_requested()) {
				mIOContext.run(error);
			}

			std::cout << "Context done with error: " << error << std::endl;
		});

		auto streamFrameRate = (double)videoStream->encoder->time_base.den / (double)videoStream->encoder->time_base.num;
		std::cout << "Grabbing: " << screenInteractor->width() << "x" << screenInteractor->height() << " @ " << streamFrameRate << " FPS" << std::endl;

		video::Converter converter;
		video::network::PacketSender packetSender;
		while (!mIOContextThread.get_stop_token().stop_requested()) {
			misc::RateSleeper rateSleeper(streamFrameRate);

//			misc::TimeMeasurement grabTM("Grab time");
			auto grabbedFrame = screenInteractor->grab();
//			grabTM.print();

			if (!nextFrame(videoStream, converter, grabbedFrame)) {
				break;
			}

			std::vector<std::tuple<ClientId, Socket*>> clientSockets;
			{
				auto guard = mClientSockets.guard();
				for (auto& [clientId, socket] : guard.get()) {
					clientSockets.emplace_back(clientId, socket.get());
				}
			}

			auto [done, socketErrors] = encodeFrameAndSend(clientSockets, videoStream, packetSender);

			{
				auto guard = mClientSockets.guard();
				for (auto& [clientId, socketError] : socketErrors) {
					if (socketError) {
						std::cout << "Removing client #" << clientId << " due to: " << socketError << std::endl;
						guard->erase(clientId);
					}
				}
			}

			if (done) {
				break;
			}

			{
				auto guard = mClientActions.guard();
				auto clientActions = std::move(guard.get());
				for (auto& clientAction : clientActions) {
					if (!screenInteractor->handleClientAction(clientAction)) {
						std::cout << "Unhandled command: " << clientAction.toString() << std::endl;
					}
				}
			}
		}

		std::cout << "Done encoding." << std::endl;

		mIOContextThread.request_stop();
		mIOContext.stop();

		if (mIOContextThread.joinable()) {
			mIOContextThread.join();
		}
	}

	void VideoServer::stop() {
		mIOContextThread.request_stop();
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
						{
							auto guard = mClientSockets.guard();
							auto clientId = mNextClientId++;
							std::cout << "Accepted client #" << clientId << ": " << socket->remote_endpoint() << std::endl;
							guard.get()[clientId] = socket;
						}

						receiveFromClient(socket, std::make_shared<client::ClientAction>());
					}
				} else {
					std::cout << "Failed to accept client due to: " << acceptFailed << std::endl;
				}

				accept(videoStream);
			}
		);
	}

	void VideoServer::receiveFromClient(std::shared_ptr<Socket> socket, std::shared_ptr<client::ClientAction> clientAction) {
		clientAction->clear();

		client::ClientAction::receiveAsync(
			socket,
			std::move(clientAction),
			[this, socket](boost::system::error_code error, std::shared_ptr<client::ClientAction> clientAction) {
				if (!error) {
					std::cout << "Got client action: " << clientAction->toString() << std::endl;

					{
						mClientActions.guard()->push_back(*clientAction);
					}

					receiveFromClient(socket, clientAction);
				}
			}
		);
	}

	bool VideoServer::nextFrame(video::OutputStream* videoStream,
								video::Converter& converter,
								const screeninteractor::GrabbedFrame& grabbedFrame) {
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

			std::vector<std::tuple<ClientId, video::network::PacketSender::AsyncResultPtr>> sendResults;
			video::network::PacketHeader header { videoStream->frame->pts };
			for (auto& [clientId, socket] : sockets) {
				sendResults.emplace_back(clientId, packetSender.sendAsync(*socket, header, packet));
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