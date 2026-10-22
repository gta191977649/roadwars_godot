#pragma once
#include "game_src/unresolved/race_ai_combat.hpp"
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event_key.hpp>

namespace roadwars {
// Host translation only. The stored values remain original DirectInput scan codes.
godot::Key original_scan_code_key(int code);
Result validate_weapon_keys(const OriginalWeaponKeys&, std::string& error);
struct OriginalWeaponModifiers {
    std::array<bool, 6> down{}; // Left/right Ctrl, Shift and Alt.
    void update(const godot::InputEventKey&);
};
void sample_weapon_keys(const OriginalWeaponKeys&, InputState&, const OriginalWeaponModifiers&);
} // namespace roadwars
