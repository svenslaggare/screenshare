#include "bit_rate_measurement.h"

namespace screenshare::misc {
    void BitRateMeasurement::add(std::uint64_t bits) {
        mTotalBits += bits;
        auto timeNow = std::chrono::high_resolution_clock::now();
        double elapsed = (double)std::chrono::duration_cast<std::chrono::microseconds>(timeNow - mLastMeasurement).count() / 1.0E6;
        if (elapsed >= 1.0) {
            mBitRate = (double)mTotalBits / elapsed;
            mLastMeasurement = timeNow;
            mTotalBits = 0;
        }
    }

    double BitRateMeasurement::bitRate() const {
        return mBitRate;
    }
}