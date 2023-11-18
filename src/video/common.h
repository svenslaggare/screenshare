#pragma once

#include <string>
#include <stdexcept>

extern "C" {
#include <libavcodec/avcodec.h>

#include <libavformat/avformat.h>

#include <libswscale/swscale.h>

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>

#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
}

namespace screenshare::video {
	void handleAVResult(int response, const std::string& errorMessage);

	std::string makeAvErrorString(int error);

	struct AVFormatContextDeleter {
		void operator()(AVFormatContext* ptr) {
			avformat_free_context(ptr);
		}
	};

	struct AVCodecContextDeleter {
		void operator()(AVCodecContext* ptr) {
			avcodec_free_context(&ptr);
		}
	};

	struct SwsContextDeleter {
		void operator()(SwsContext* ptr) {
			sws_freeContext(ptr);
		}
	};

	struct AVFrameDeleter {
		void operator()(AVFrame* ptr) {
			av_frame_free(&ptr);
		}
	};

	struct AVFrameWithDataDeleter {
		void operator()(AVFrame* ptr) {
			av_freep(&ptr->data[0]);
			av_frame_free(&ptr);
		}
	};

	struct AVPacketDeleter {
		void operator()(AVPacket* ptr) {
			av_packet_free(&ptr);
		}
	};
}