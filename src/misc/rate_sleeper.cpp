#include "rate_sleeper.h"

#include <thread>

namespace screenshare::misc {
	RateSleeper::RateSleeper(double rate)
		:  mStartTime(std::chrono::high_resolution_clock::now()),
		   mRate(rate) {

	}

	RateSleeper::~RateSleeper() {
		std::this_thread::sleep_for(std::chrono::microseconds((long)(
			std::max(0.0, (1.0E6 / mRate) - (double)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - mStartTime).count())
		)));
	}
}