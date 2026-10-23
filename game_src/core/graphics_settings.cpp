#include "game_src/core/graphics_settings.hpp"

#include <algorithm>
#include <cmath>

namespace roadwars {
namespace {
template <class T> bool contains(const std::vector<T>& values, T value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}
bool in_range(float value, float low, float high) {
    return std::isfinite(value) && value >= low && value <= high;
}
} // namespace

Result validate_graphics_settings(const GraphicsSettings& s, const GraphicsCapabilities& c,
                                 const GraphicsSettings& current) {
    if (s.monitor < 0 || static_cast<std::size_t>(s.monitor) >= c.monitors.size() ||
        s.width < 1 || s.width > 32768 || s.height < 1 || s.height > 32768 ||
        s.fps_limit < 0 || s.fps_limit > 1000 || !in_range(s.render_scale, .5f, 1.f) ||
        !in_range(s.view_distance, .4f, 1.f) || !in_range(s.effects, .33f, 1.f) ||
        !in_range(s.skid_marks, .33f, 1.f) || s.car_detail < 0 || s.car_detail > 2)
        return Result::invalid_argument;
    if (s.window_mode != WindowMode::windowed && s.window_mode != WindowMode::borderless &&
        s.window_mode != WindowMode::fullscreen)
        return Result::invalid_argument;
    if (s.texture_quality != TextureQuality::high && s.texture_quality != TextureQuality::medium &&
        s.texture_quality != TextureQuality::low)
        return Result::invalid_argument;
    if (s.texture_filtering != TextureFiltering::linear &&
        s.texture_filtering != TextureFiltering::anisotropic_2x &&
        s.texture_filtering != TextureFiltering::anisotropic_4x &&
        s.texture_filtering != TextureFiltering::anisotropic_8x &&
        s.texture_filtering != TextureFiltering::anisotropic_16x)
        return Result::invalid_argument;
    if (!contains(c.backends, s.backend) || !contains(c.anti_aliasing, s.aa) ||
        (!c.texture_quality && s.texture_quality != current.texture_quality) ||
        (!c.texture_filtering && s.texture_filtering != current.texture_filtering) ||
        (!c.car_detail && s.car_detail != current.car_detail))
        return Result::unavailable;
    return Result::ok;
}

bool graphics_display_changed(const GraphicsSettings& a, const GraphicsSettings& b) {
    return a.monitor != b.monitor || a.width != b.width || a.height != b.height ||
           a.window_mode != b.window_mode;
}

GraphicsSettingsTransaction::GraphicsSettingsTransaction(GraphicsSettingsHost& host)
    : host_(host), confirmed_(host.current_settings()), draft_(confirmed_), candidate_(confirmed_) {}

Result GraphicsSettingsTransaction::apply(double now) {
    if (pending_)
        return Result::unavailable;
    if (!std::isfinite(now) || now < last_time_)
        return Result::invalid_argument;
    last_time_ = now;
    const auto validation = validate_graphics_settings(draft_, host_.capabilities(), confirmed_);
    if (validation != Result::ok)
        return validation;
    const auto result = host_.apply_live(draft_);
    if (result != Result::ok)
        return result;
    candidate_ = draft_;
    pending_ = true;
    deadline_ = now + kConfirmationSeconds;
    if (graphics_display_changed(confirmed_, candidate_))
        return Result::ok;
    const auto saved = confirm();
    if (saved == Result::ok)
        return Result::ok;
    const auto restored = revert();
    return restored == Result::ok ? saved : restored;
}

Result GraphicsSettingsTransaction::confirm() {
    if (!pending_)
        return Result::invalid_argument;
    const auto result = host_.persist(candidate_);
    if (result != Result::ok)
        return result;
    confirmed_ = candidate_;
    draft_ = confirmed_;
    pending_ = false;
    return Result::ok;
}

Result GraphicsSettingsTransaction::revert() {
    if (!pending_) {
        discard_draft();
        return Result::ok;
    }
    const auto result = host_.apply_live(confirmed_);
    if (result != Result::ok)
        return result;
    pending_ = false;
    discard_draft();
    return Result::ok;
}

Result GraphicsSettingsTransaction::update(double now) {
    if (!std::isfinite(now) || now < last_time_)
        return Result::invalid_argument;
    last_time_ = now;
    if (pending_ && now >= deadline_)
        return revert();
    return Result::ok;
}

double GraphicsSettingsTransaction::seconds_remaining(double now) const {
    return pending_ && std::isfinite(now) ? std::max(0.0, deadline_ - now) : 0;
}
} // namespace roadwars
