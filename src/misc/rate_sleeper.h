#pragma once
#include <chrono>

namespace screenshare::misc {
	class RateSleeper {
	private:
		std::chrono::high_resolution_clock::time_point mStartTime;
		double mRate;
	public:
		explicit RateSleeper(double rate);
		~RateSleeper();
	};
}