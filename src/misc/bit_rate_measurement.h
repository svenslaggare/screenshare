#pragma once
#include <cstdint>
#include <chrono>

namespace screenshare::misc {
    class BitRateMeasurement {
    private:
        std::uint64_t mTotalBits = 0;
        double mBitRate = 0.0;
        std::chrono::high_resolution_clock::time_point mLastMeasurement = std::chrono::high_resolution_clock::now();
    public:
        void add(std::uint64_t bits);

        double bitRate() const;
    };
}