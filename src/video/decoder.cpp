#include "decoder.h"
#include "network.h"

namespace screenshare::video {
	namespace {
		constexpr AVPixelFormat CONVERT_RGB_FORMAT = AV_PIX_FMT_RGB24;

		void printDecodedFrame(AVCodecContext* codecContext, AVFrame* frame) {
			std::cout
				<< "Frame " << codecContext->frame_number
				<< " (type=" << av_get_picture_type_char(frame->pict_type)
				<< ", size=" << frame->pkt_size << " bytes, format=" << frame->format << ") pts " << frame->pts << " key_frame " << frame->key_frame << " frame number: " << frame->coded_picture_number << "]"
				<< std::endl;
		}
	}

	int PacketDecoder::decode(AVPacket* packet,
							  AVCodecContext* codecContext,
							  AVFrame* frame,
							  std::function<void (AVCodecContext*, AVFrame*)> callback) {
		if (auto response = avcodec_send_packet(codecContext, packet) < 0) {
			std::cout << "Error while sending a packet to the decoder: " << makeAvErrorString(response) << std::endl;
			return response;
		}

		while (true) {
			auto response = avcodec_receive_frame(codecContext, frame);
			if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
				break;
			} else if (response < 0) {
				std::cout << "Error while receiving a frame from the decoder: " << makeAvErrorString(response) << std::endl;
				return response;
			}

//			printDecodedFrame(codecContext, frame);

			if (!mRGBFrame) {
				mRGBFrame = decltype(mRGBFrame) { av_frame_alloc() };

				mRGBFrame->width = frame->width;
				mRGBFrame->height = frame->height;
				mRGBFrame->format = CONVERT_RGB_FORMAT;

				av_image_alloc(
					mRGBFrame->data,
					mRGBFrame->linesize,
					mRGBFrame->width,
					mRGBFrame->height,
					(AVPixelFormat)mRGBFrame->format,
					1
				);
			}

			if (!mConversion) {
				mConversion = decltype(mConversion) {
					sws_getContext(
						frame->width,
						frame->height,
						(AVPixelFormat)frame->format,

						mRGBFrame->width,
						mRGBFrame->height,
						(AVPixelFormat)mRGBFrame->format,

						SWS_FAST_BILINEAR,
						nullptr,
						nullptr,
						nullptr
					)
				};
			}

			if (sws_scale(mConversion.get(), frame->data, frame->linesize, 0, frame->height, mRGBFrame->data, mRGBFrame->linesize) >= 0) {
				callback(codecContext, mRGBFrame.get());
			}
		}

		return 0;
	}

	int PacketDecoder::decode(AVPacket* packet,
							  AVCodecContext* codecContext,
							  AVFrame* frame,
							  std::uint8_t* destination,
							  std::function<void (AVCodecContext*)> callback) {
		if (auto response = avcodec_send_packet(codecContext, packet) < 0) {
			std::cout << "Error while sending a packet to the decoder: " << makeAvErrorString(response) << std::endl;
			return response;
		}

		while (true) {
			auto response = avcodec_receive_frame(codecContext, frame);
			if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
				break;
			} else if (response < 0) {
				std::cout << "Error while receiving a frame from the decoder: " << makeAvErrorString(response) << std::endl;
				return response;
			}

//			printDecodedFrame(codecContext, frame);

			if (!mConversion) {
				mConversion = decltype(mConversion) {
					sws_getContext(
						frame->width,
						frame->height,
						(AVPixelFormat)frame->format,

						frame->width,
						frame->height,
						CONVERT_RGB_FORMAT,

						SWS_FAST_BILINEAR,
						nullptr,
						nullptr,
						nullptr
					)
				};
			}

			std::uint8_t* destinationPtrs[AV_NUM_DATA_POINTERS] = {};
			int destinationLineSizes[AV_NUM_DATA_POINTERS] = {};
			destinationPtrs[0] = destination;
			destinationLineSizes[0] = frame->width * 3;

			if (sws_scale(mConversion.get(), frame->data, frame->linesize, 0, frame->height, destinationPtrs, destinationLineSizes) >= 0) {
				callback(codecContext);
			}
		}

		return 0;
	}
}