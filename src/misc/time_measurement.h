#pragma once

#include <string>
#include <chrono>
#include <iostream>

namespace screenshare::misc {
	class TimeMeasurement {
	private:
		std::string mPattern;
		std::chrono::high_resolution_clock::time_point mStartTime;
		bool mPrinted = false;
	public:
		explicit TimeMeasurement(std::string pattern);
		~TimeMeasurement();

		void changePattern(std::string pattern);

		double elapsedMilliseconds() const;
		void print();
	};
}