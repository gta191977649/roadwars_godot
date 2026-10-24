#pragma once
#include "game_src/unresolved/frontend_canvas.hpp"

namespace roadwars {
// Shared portable coordinates from 004afd70 and 004b090a, including integer midpoints.
struct FrontendDialogLayout {
    FrontendSprite panel_sprite, button_sprite;
    HudRect panel;
    std::array<HudRect, 2> buttons{};
    std::array<std::array<int, 2>, 2> button_centers{};
    int message_x{320}, message_y{};
    std::size_t button_count{};
    float panel_alpha{};
    int hit(float x, float y) const {
        for (std::size_t i = 0; i < button_count; ++i) {
            const auto& r = buttons[i];
            if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height)
                return static_cast<int>(i);
        }
        return -1;
    }
};
inline Result frontend_dialog_layout(FrontendCanvas& canvas, bool notice,
                                     FrontendDialogLayout& output) {
    output = {};
    auto result = canvas.asset("Screens/16DialogBackground.bmp", output.panel_sprite);
    if (result != Result::ok)
        return result;
    result = canvas.asset("Screens/16DialogButton.bmp", output.button_sprite);
    if (result != Result::ok)
        return result;
    const auto& panel = output.panel_sprite;
    const auto& button = output.button_sprite;
    output.panel = {320 - panel.width / 2, 240 - panel.height / 2, panel.width, panel.height};
    output.message_y = output.panel.y + 39;
    output.button_count = notice ? 1 : 2;
    output.panel_alpha = notice ? 1.f : .6f;
    const int top = output.panel.y + 55;
    if (notice)
        output.buttons[0] = {320 + button.width / 2 - button.width, top, button.width,
                             button.height};
    else {
        output.buttons[0] = {315 - button.width, top, button.width, button.height};
        output.buttons[1] = {325, top, button.width, button.height};
    }
    for (std::size_t i = 0; i < output.button_count; ++i) {
        const auto& r = output.buttons[i];
        output.button_centers[i] = {(r.x + r.x + r.width) / 2, (r.y + r.y + r.height) / 2};
    }
    return Result::ok;
}
} // namespace roadwars
