#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace roadwars::extensions {
// Wall-clock render callback intervals, never a simulation time source.
class FrameTiming {
  public:
    static constexpr std::size_t kCapacity = 600;
    struct Summary {
        float mean{}, p95{}, p99{}, maximum{};
        std::size_t count{}, late{};
    };
    void record(float milliseconds) {
        if (!std::isfinite(milliseconds) || milliseconds <= 0)
            return;
        samples_[next_] = milliseconds;
        next_ = (next_ + 1) % kCapacity;
        count_ = std::min(count_ + 1, kCapacity);
    }
    Summary summary(float target_ms) const {
        Summary result;
        result.count = count_;
        if (!count_)
            return result;
        auto sorted = samples_;
        for (std::size_t i = 0; i < count_; ++i) {
            result.mean += samples_[i];
            result.late += target_ms > 0 && samples_[i] > target_ms * 1.5f;
        }
        result.mean /= static_cast<float>(count_);
        std::sort(sorted.begin(), sorted.begin() + count_);
        result.p95 = sorted[(count_ * 95 + 99) / 100 - 1];
        result.p99 = sorted[(count_ * 99 + 99) / 100 - 1];
        result.maximum = sorted[count_ - 1];
        return result;
    }
    const float* data() const { return samples_.data(); }
    int count() const { return static_cast<int>(count_); }
    int offset() const { return count_ == kCapacity ? static_cast<int>(next_) : 0; }

  private:
    std::array<float, kCapacity> samples_{};
    std::size_t next_{}, count_{};
};
} // namespace roadwars::extensions
