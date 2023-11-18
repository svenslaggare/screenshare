#include "common.h"

namespace screenshare::video {
	void handleAVResult(int response, const std::string& errorMessage) {
		if (response < 0) {
			throw std::runtime_error(errorMessage);
		}
	}

	std::string makeAvErrorString(int error) {
		char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
		return av_make_error_string(errorBuffer, AV_ERROR_MAX_STRING_SIZE, error);
	}
}