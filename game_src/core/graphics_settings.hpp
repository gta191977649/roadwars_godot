#pragma once

#include "game_src/interfaces/graphics_settings.hpp"

namespace roadwars {
Result validate_graphics_settings(const GraphicsSettings& settings,
                                 const GraphicsCapabilities& capabilities,
                                 const GraphicsSettings& current);
bool graphics_display_changed(const GraphicsSettings& a, const GraphicsSettings& b);

class GraphicsSettingsTransaction {
  public:
    static constexpr double kConfirmationSeconds = 15;
    explicit GraphicsSettingsTransaction(GraphicsSettingsHost& host);
    GraphicsSettings& draft() { return draft_; }
    const GraphicsSettings& draft() const { return draft_; }
    const GraphicsSettings& confirmed() const { return confirmed_; }
    bool pending_confirmation() const { return pending_; }
    Result apply(double monotonic_seconds);
    Result confirm();
    Result revert();
    Result update(double monotonic_seconds);
    double seconds_remaining(double monotonic_seconds) const;
    void discard_draft() { draft_ = confirmed_; }
    void restore_defaults() { draft_ = host_.default_settings(); }

  private:
    GraphicsSettingsHost& host_;
    GraphicsSettings confirmed_, draft_, candidate_;
    bool pending_{};
    double deadline_{}, last_time_{};
};
} // namespace roadwars
