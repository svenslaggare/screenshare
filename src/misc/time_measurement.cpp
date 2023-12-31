#include "time_measurement.h"

namespace screenshare::misc {
	TimeMeasurement::TimeMeasurement(std::string pattern)
		: mPattern(std::move(pattern)),
		  mStartTime(std::chrono::high_resolution_clock::now()) {

	}

	TimeMeasurement::~TimeMeasurement() {
		print();
	}

	void TimeMeasurement::changePattern(std::string pattern) {
		mPattern = std::move(pattern);
	}

	double TimeMeasurement::elapsedMilliseconds() const {
		return (double)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - mStartTime).count() / 1000.0;
	}

	void TimeMeasurement::print() {
		if (!mPrinted) {
			std::cout << mPattern << ": " << elapsedMilliseconds() << " ms" << std::endl;
			mPrinted = true;
		}
	}

	double elapsedSeconds(const timespec& x, const timespec& y) {
		return (double)(x.tv_sec - y.tv_sec) + (double)(x.tv_nsec - y.tv_nsec) / 1.0E9;
	}
}