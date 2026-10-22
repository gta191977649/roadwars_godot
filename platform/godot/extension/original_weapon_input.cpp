#include "platform/godot/extension/original_weapon_input.hpp"
#include <godot_cpp/classes/input.hpp>

namespace roadwars {
godot::Key original_scan_code_key(int code) {
    if (code >= 2 && code <= 11)
        return static_cast<godot::Key>("1234567890"[code - 2]);
    if (code >= 16 && code <= 25)
        return static_cast<godot::Key>("QWERTYUIOP"[code - 16]);
    if (code >= 30 && code <= 38)
        return static_cast<godot::Key>("ASDFGHJKL"[code - 30]);
    if (code >= 44 && code <= 50)
        return static_cast<godot::Key>("ZXCVBNM"[code - 44]);
    if (code >= 59 && code <= 68)
        return static_cast<godot::Key>(godot::KEY_F1 + code - 59);
    switch (code) {
    case 1:
        return godot::KEY_ESCAPE;
    case 12:
        return godot::KEY_MINUS;
    case 13:
        return godot::KEY_EQUAL;
    case 14:
        return godot::KEY_BACKSPACE;
    case 15:
        return godot::KEY_TAB;
    case 26:
        return godot::KEY_BRACKETLEFT;
    case 27:
        return godot::KEY_BRACKETRIGHT;
    case 28:
        return godot::KEY_ENTER;
    case 29:
    case 157:
        return godot::KEY_CTRL;
    case 39:
        return godot::KEY_SEMICOLON;
    case 40:
        return godot::KEY_APOSTROPHE;
    case 41:
        return godot::KEY_QUOTELEFT;
    case 42:
    case 54:
        return godot::KEY_SHIFT;
    case 43:
        return godot::KEY_BACKSLASH;
    case 51:
        return godot::KEY_COMMA;
    case 52:
        return godot::KEY_PERIOD;
    case 53:
        return godot::KEY_SLASH;
    case 55:
        return godot::KEY_KP_MULTIPLY;
    case 56:
    case 184:
        return godot::KEY_ALT;
    case 57:
        return godot::KEY_SPACE;
    case 58:
        return godot::KEY_CAPSLOCK;
    case 69:
        return godot::KEY_NUMLOCK;
    case 70:
        return godot::KEY_SCROLLLOCK;
    case 71:
        return godot::KEY_KP_7;
    case 72:
        return godot::KEY_KP_8;
    case 73:
        return godot::KEY_KP_9;
    case 74:
        return godot::KEY_KP_SUBTRACT;
    case 75:
        return godot::KEY_KP_4;
    case 76:
        return godot::KEY_KP_5;
    case 77:
        return godot::KEY_KP_6;
    case 78:
        return godot::KEY_KP_ADD;
    case 79:
        return godot::KEY_KP_1;
    case 80:
        return godot::KEY_KP_2;
    case 81:
        return godot::KEY_KP_3;
    case 82:
        return godot::KEY_KP_0;
    case 83:
        return godot::KEY_KP_PERIOD;
    case 87:
        return godot::KEY_F11;
    case 88:
        return godot::KEY_F12;
    case 156:
        return godot::KEY_KP_ENTER;
    case 181:
        return godot::KEY_KP_DIVIDE;
    case 199:
        return godot::KEY_HOME;
    case 200:
        return godot::KEY_UP;
    case 201:
        return godot::KEY_PAGEUP;
    case 203:
        return godot::KEY_LEFT;
    case 205:
        return godot::KEY_RIGHT;
    case 207:
        return godot::KEY_END;
    case 208:
        return godot::KEY_DOWN;
    case 209:
        return godot::KEY_PAGEDOWN;
    case 210:
        return godot::KEY_INSERT;
    case 211:
        return godot::KEY_DELETE;
    default:
        return godot::KEY_NONE;
    }
}
Result validate_weapon_keys(const OriginalWeaponKeys& keys, std::string& error) {
    for (int code : {keys.current, keys.front, keys.side, keys.rear, keys.nitrous, keys.cycle})
        if (code != -1 && original_scan_code_key(code) == godot::KEY_NONE) {
            error = "Unsupported original weapon DirectInput scan code: " + std::to_string(code);
            return Result::unavailable;
        }
    return Result::ok;
}
void OriginalWeaponModifiers::update(const godot::InputEventKey& event) {
    const auto code = event.get_physical_keycode();
    const auto location = event.get_location();
    if (location == godot::KEY_LOCATION_UNSPECIFIED)
        return;
    const std::size_t side = location == godot::KEY_LOCATION_RIGHT ? 1 : 0;
    if (code == godot::KEY_CTRL)
        down[side] = event.is_pressed();
    else if (code == godot::KEY_SHIFT)
        down[2 + side] = event.is_pressed();
    else if (code == godot::KEY_ALT)
        down[4 + side] = event.is_pressed();
}
void sample_weapon_keys(const OriginalWeaponKeys& keys, InputState& input,
                        const OriginalWeaponModifiers& modifiers) {
    const auto* keyboard = godot::Input::get_singleton();
    const auto down = [&](int code) {
        switch (code) {
        case 29:
            return modifiers.down[0];
        case 157:
            return modifiers.down[1];
        case 42:
            return modifiers.down[2];
        case 54:
            return modifiers.down[3];
        case 56:
            return modifiers.down[4];
        case 184:
            return modifiers.down[5];
        default:
            break;
        }
        const auto key = original_scan_code_key(code);
        return key != godot::KEY_NONE && keyboard->is_physical_key_pressed(key);
    };
    input.weapon_down = {down(keys.front), down(keys.side), down(keys.rear)};
    input.nitrous_down = down(keys.nitrous);
    input.current_weapon_down = down(keys.current);
    input.cycle_weapon_down = down(keys.cycle);
}
} // namespace roadwars
