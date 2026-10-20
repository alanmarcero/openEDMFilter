#pragma once

#include <array>

namespace openedm::core {

// 2x oversampler with a linear-phase, Kaiser-windowed halfband FIR. Stopband is about -80 dB.
class Oversampler {
public:
    static constexpr int kFactor = 2;
    static constexpr int kMaxChannels = 2;
    static constexpr int kBranchTaps = 48;
    // Delay of upsample() followed by downsample(), in base-rate samples.
    static constexpr int kLatency = kBranchTaps - 1;

    using Frame = std::array<float, kFactor>;

    Oversampler() noexcept;
    void reset() noexcept;
    [[nodiscard]] Frame upsample(int channel, float input) noexcept;
    [[nodiscard]] float downsample(int channel, const Frame& input) noexcept;

private:
    struct History {
        // Each sample is written twice so a convolution reads kBranchTaps contiguous values from head.
        std::array<float, 2 * kBranchTaps> samples{};
        int head = 0;

        void push(float sample) noexcept;
        [[nodiscard]] float at(int age) const noexcept;
    };

    [[nodiscard]] float convolve(const History& history) const noexcept;

    // Taps at odd offsets from the centre. The even offsets are zero, except the centre tap of 0.5.
    std::array<float, kBranchTaps> taps_{};
    std::array<History, kMaxChannels> upHistory_{};
    std::array<History, kMaxChannels> downHistory_{};
    std::array<History, kMaxChannels> downCentre_{};
};

} // namespace openedm::core
