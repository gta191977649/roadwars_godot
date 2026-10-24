#pragma once
#include <algorithm>
#include <cmath>

namespace roadwars {
// Portable reconstruction of the demo loop at00406fc0. The0.5-second demo
// exit fade is a requested presentation extension, not recovered timing.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00406fc0
class TitleAttract {
  public:
    void start() {
        *this = {};
        active_ = true;
    }
    void dismiss() {
        if (active_)
            exiting_ = true;
    }
    bool advance(double delta) {
        if (!active_ || !std::isfinite(delta) || delta < 0)
            return false;
        if (exiting_) {
            exit_time_ = std::min(.5, exit_time_ + delta);
            return false;
        }
        elapsed_ += delta;
        camera_time_ += delta;
        if (elapsed_ + 1e-9 >= 45) {
            exiting_ = true;
            return false;
        }
        if (camera_time_ > 5) {
            camera_time_ = 0;
            return true;
        }
        return false;
    }
    bool active() const { return active_; }
    bool exiting() const { return exiting_; }
    bool black() const { return exiting_ && exit_time_ >= .5; }
    double elapsed() const { return elapsed_; }
    float exit_opacity() const { return static_cast<float>(exit_time_ / .5); }
  private:
    bool active_{}, exiting_{};
    double elapsed_{}, camera_time_{}, exit_time_{};
};
} // namespace roadwars
