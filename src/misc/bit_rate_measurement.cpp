#include "bit_rate_measurement.h"

#include <tuple>
#include <iostream>
#include <numeric>

namespace screenshare::misc {
    void BitRateMeasurement::add(std::uint64_t bits) {
        mTotalBits += bits;
        auto timeNow = std::chrono::high_resolution_clock::now();
        auto elapsed = (double)std::chrono::duration_cast<std::chrono::microseconds>(timeNow - mLastMeasurement).count() / 1.0E6;

        if (elapsed >= 1.0) {
            if (mMeasurements.size() >= 5) {
                mMeasurements.pop_front();
            }

            mMeasurements.emplace_back(mTotalBits, elapsed);

            mLastMeasurement = timeNow;
            mTotalBits = 0;

            auto [sumBits, sumElapsed] = std::accumulate(
                mMeasurements.begin(),
                mMeasurements.end(),
                std::tuple(0, 0.0),
                [](auto sum, auto& current) {
                    return std::make_tuple(
                        std::get<0>(sum) + std::get<0>(current),
                        std::get<1>(sum) + std::get<1>(current)
                    );
                }
            );

            mAverageBitRate = (double)sumBits / sumElapsed;
        }
    }

    double BitRateMeasurement::averageBitRate() const {
        return mAverageBitRate;
    }
}