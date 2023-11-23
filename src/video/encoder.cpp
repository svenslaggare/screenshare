#include "encoder.h"

namespace screenshare::video {
	namespace {
		AVFrame* allocFrame(enum AVPixelFormat pixelFormat, int width, int height) {
			auto frame = av_frame_alloc();
			if (!frame) {
				return nullptr;
			}

			frame->format = pixelFormat;
			frame->width = width;
			frame->height = height;

			//allocate the buffers for the frame data
			if (av_frame_get_buffer(frame, 0) < 0) {
				std::cout << "av_frame_get_buffer failed" << std::endl;
				return nullptr;
			}

			return frame;
		}
	}

	bool Converter::convert(int sourceWidth, int sourceHeight, AVPixelFormat sourceFormat, std::uint8_t* source, int sourceLineSize,
							int destinationWidth, int destinationHeight, AVPixelFormat destinationFormat, std::uint8_t** destination, int* destinationLineSize) {
		if (!mConversion) {
			mConversion = decltype(mConversion) {
				sws_getContext(
					sourceWidth,
					sourceHeight,
					sourceFormat,

					destinationWidth,
					destinationHeight,
					destinationFormat,

					SWS_FAST_BILINEAR,
					nullptr,
					nullptr,
					nullptr
				)
			};
		}

		std::uint8_t* sourceDataPtrs[AV_NUM_DATA_POINTERS] = {};
		int sourceLineSizes[AV_NUM_DATA_POINTERS] = {};
		sourceDataPtrs[0] = source;
		sourceLineSizes[0] = sourceLineSize;

		return sws_scale(
			mConversion.get(),
			sourceDataPtrs, sourceLineSizes, 0, sourceHeight,
			destination, destinationLineSize
		) >= 0;
	}

	VideoReader::VideoReader(std::string_view filename) {
		mFormatContext = avformat_alloc_context();
		if (!mFormatContext) {
			throw std::runtime_error("ERROR could not allocate memory for Format Context");
		}

		std::cout << "opening the input file (" << filename<< ") and loading format (container) header" << std::endl;

		handleAVResult(avformat_open_input(&mFormatContext, filename.data(), nullptr, nullptr), "ERROR could not open the file");
		std::cout << "format " << mFormatContext->iformat->name << ", duration " << mFormatContext->duration << " us, bit_rate " << mFormatContext->bit_rate << std::endl;

		std::cout << "finding stream info from format" << std::endl;
		handleAVResult(avformat_find_stream_info(mFormatContext, nullptr), "ERROR could not get the stream info");

		AVCodec* codec = nullptr;
		for (int i = 0; i < mFormatContext->nb_streams; i++) {
			auto localCodecParameters = mFormatContext->streams[i]->codecpar;
			std::cout << "AVStream->time_base before open coded " << mFormatContext->streams[i]->time_base.num << "/" << mFormatContext->streams[i]->time_base.den << std::endl;
			std::cout << "AVStream->r_frame_rate before open coded " << mFormatContext->streams[i]->r_frame_rate.num << "/" << mFormatContext->streams[i]->r_frame_rate.den << std::endl;
			std::cout << "AVStream->start_time " << mFormatContext->streams[i]->start_time << std::endl;
			std::cout << "AVStream->duration " << mFormatContext->streams[i]->duration << std::endl;

			std::cout << "finding the proper decoder (CODEC)" << std::endl;

			auto localCodec = avcodec_find_decoder(localCodecParameters->codec_id);
			if (localCodec == nullptr) {
				std::cout << "ERROR unsupported codec!" << std::endl;
				continue;
			}

			if (localCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
				if (mVideoStreamIndex == -1) {
					mVideoStreamIndex = i;
					codec = localCodec;
					mCodecParameters = localCodecParameters;
				}

				std::cout << "Video Codec: resolution " << localCodecParameters->width << " x " << localCodecParameters->height << std::endl;
			} else if (localCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
				std::cout << "Audio Codec: " << localCodecParameters->channels << " channels, sample rate " << localCodecParameters->sample_rate << std::endl;
			}

			std::cout << "\tCodec " << localCodec->name << " ID " << localCodec->id << " bit_rate " << localCodecParameters->bit_rate << std::endl;
		}

		if (mVideoStreamIndex == -1) {
			std::stringstream errorMessage;
			errorMessage << "File " << filename << " does not contain a video stream!";
			throw std::runtime_error(errorMessage.str());
		}

		mCodecContext = decltype(mCodecContext) { avcodec_alloc_context3(codec) };
		if (!mCodecContext) {
			throw std::runtime_error("failed to allocated memory for AVCodecContext");
		}

		handleAVResult(avcodec_parameters_to_context(mCodecContext.get(), mCodecParameters), "failed to copy codec params to codec context");
		handleAVResult(avcodec_open2(mCodecContext.get(), codec, nullptr), "failed to open codec through avcodec_open2");
	}

	VideoReader::~VideoReader() {
		avformat_close_input(&mFormatContext);
	}

	int VideoReader::videoStreamIndex() const {
		return mVideoStreamIndex;
	}

	AVFormatContext* VideoReader::formatContext() {
		return mFormatContext;
	}

	AVCodecContext* VideoReader::codecContext() {
		return mCodecContext.get();
	}

	AVCodecParameters* VideoReader::codecParameters() {
		return mCodecParameters;
	}

	VideoEncoder::VideoEncoder(const std::string& format) {
		AVFormatContext* outputFormatContextPtr;
		avformat_alloc_output_context2(&outputFormatContextPtr, nullptr, format.c_str(), nullptr);
		if (!outputFormatContextPtr) {
			throw std::runtime_error("Failed to create output format context.");
		}

		mOutputFormatContext = decltype(mOutputFormatContext) { outputFormatContextPtr };
	}

	AVFormatContext* VideoEncoder::outputFormatContext() {
		return mOutputFormatContext.get();
	}

	const std::vector<OutputStream>& VideoEncoder::streams() const {
		return mStreams;
	}

	OutputStream* VideoEncoder::addVideoStream(int width, int height, int frameRate, AVDictionary* options) {
		auto outputFormat = mOutputFormatContext->oformat;

		auto videoStream = createVideoStream(
			mOutputFormatContext.get(),
			outputFormat->video_codec,
			width, height, frameRate, options
		);
		if (!videoStream) {
			return nullptr;
		}

		mStreams.push_back(std::move(*videoStream));
		return &mStreams.back();
	}

	std::optional<OutputStream> createOutputStream(AVFormatContext* outputFormatContext, AVCodecID codecId) {
		//find the encoder
		auto codec = avcodec_find_encoder(codecId);
		if (!codec) {
			std::cout << "Could not find encoder for: " << avcodec_get_name(codecId) << std::endl;
			return {};
		}

		OutputStream outputStream;
		outputStream.codec = codec;

		outputStream.packet = decltype(outputStream.packet){ av_packet_alloc() };
		if (!outputStream.packet) {
			std::cout << "Could not allocate AVPacket" << std::endl;
			return {};
		}

		outputStream.stream = avformat_new_stream(outputFormatContext, nullptr);
		if (!outputStream.stream) {
			std::cout << "Could not allocate stream" << std::endl;
			return {};
		}

		outputStream.stream->id = (int)outputFormatContext->nb_streams - 1;
		outputStream.encoder = decltype(outputStream.encoder){ avcodec_alloc_context3(codec) };
		if (!outputStream.encoder) {
			std::cout << "Could not alloc an encoding context" << std::endl;
			return {};
		}

		return { std::move(outputStream) };
	}

	std::optional<OutputStream> createVideoStream(AVFormatContext* outputFormatContext, AVCodecID codecId,
												  int width, int height, int frameRate, AVDictionary* options) {
		auto outputStream = createOutputStream(outputFormatContext, codecId);
		if (!outputStream) {
			return {};
		}

		switch (outputStream->codec->type) {
			case AVMEDIA_TYPE_VIDEO:
				outputStream->encoder->codec_id = codecId;

				outputStream->encoder->bit_rate = (width * height) * 2;

				//Resolution must be a multiple of two.
				outputStream->encoder->width = width;
				outputStream->encoder->height = height;

				outputStream->stream->time_base = (AVRational){ 1, frameRate };
				outputStream->encoder->time_base = outputStream->stream->time_base;
				outputStream->encoder->framerate = outputStream->stream->time_base;

				outputStream->encoder->pix_fmt = AV_PIX_FMT_YUV420P;

				outputStream->encoder->gop_size = 15;
				outputStream->encoder->max_b_frames = 1;
				handleAVResult(av_opt_set(outputStream->encoder->priv_data, "preset", "ultrafast", 0), "Failed to set preset");

//				handleAVResult(av_opt_set(outputStream->encoder->priv_data, "rc-lookahead", "0", 0), "Failed to set rc_lookahead");
				break;
			default:
				break;
		}

		//Some formats want stream headers to be separate.
		if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
			outputStream->encoder->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}

		auto codecContext = outputStream->encoder.get();

		AVDictionary* opt = nullptr;
		av_dict_copy(&opt, options, 0);

		// open the codec
		if (avcodec_open2(codecContext, outputStream->codec, &opt) < 0) {
			av_dict_free(&opt);
			std::cout << "avcodec_open2 failed" << std::endl;
			return {};
		}

		av_dict_free(&opt);

		// allocate and init a re-usable frame
		outputStream->frame = decltype(outputStream->frame) { allocFrame(codecContext->pix_fmt, codecContext->width, codecContext->height) };
		if (!outputStream->frame) {
			std::cout << "Could not allocate video frame" << std::endl;
			return {};
		}

		// copy the stream parameters to the muxer
		if (avcodec_parameters_from_context(outputStream->stream->codecpar, codecContext) < 0) {
			std::cout << "Could not copy the stream parameters" << std::endl;
			return { };
		}

		return { std::move(outputStream) };
	}
}