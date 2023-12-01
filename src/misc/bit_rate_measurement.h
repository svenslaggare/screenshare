#pragma once
#include <cstdint>
#include <chrono>
#include <deque>

namespace screenshare::misc {
    class BitRateMeasurement {
    private:
        std::uint64_t mTotalBits = 0;
        std::chrono::high_resolution_clock::time_point mLastMeasurement = std::chrono::high_resolution_clock::now();

        std::deque<std::tuple<std::uint64_t, double>> mMeasurements;
        double mAverageBitRate = 0.0;
    public:
        void add(std::uint64_t bits);

        double averageBitRate() const;
    };
}