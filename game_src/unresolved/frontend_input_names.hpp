#pragma once
#include <array>
#include <string_view>
#include <utility>

namespace roadwars {
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00486ac0
inline std::string_view frontend_keyboard_name(int code) {
    constexpr std::array<std::pair<int, std::string_view>, 78> names{{
        {2, "1"},
        {3, "2"},
        {4, "3"},
        {5, "4"},
        {6, "5"},
        {7, "6"},
        {8, "7"},
        {9, "8"},
        {10, "9"},
        {11, "0"},
        {30, "A"},
        {48, "B"},
        {46, "C"},
        {32, "D"},
        {18, "E"},
        {33, "F"},
        {34, "G"},
        {35, "H"},
        {23, "I"},
        {36, "J"},
        {37, "K"},
        {38, "L"},
        {50, "M"},
        {49, "N"},
        {24, "O"},
        {25, "P"},
        {16, "Q"},
        {19, "R"},
        {31, "S"},
        {20, "T"},
        {22, "U"},
        {47, "V"},
        {17, "W"},
        {45, "X"},
        {21, "Y"},
        {44, "Z"},
        {82, "NUMPAD 0"},
        {79, "NUMPAD 1"},
        {80, "NUMPAD 2"},
        {81, "NUMPAD 3"},
        {75, "NUMPAD 4"},
        {76, "NUMPAD 5"},
        {77, "NUMPAD 6"},
        {71, "NUMPAD 7"},
        {72, "NUMPAD 8"},
        {73, "NUMPAD 9"},
        {12, "-"},
        {13, "="},
        {15, "TAB"},
        {26, "["},
        {27, "]"},
        {29, "LEFT CTRL"},
        {39, ";"},
        {40, "'"},
        {42, "LEFT SHIFT"},
        {52, "."},
        {51, ","},
        {54, "RIGHT SHIFT"},
        {53, "/"},
        {200, "UP ARROW"},
        {208, "DOWN ARROW"},
        {205, "RIGHT ARROW"},
        {203, "LEFT ARROW"},
        {210, "INSERT"},
        {201, "PAGE UP"},
        {209, "PAGE DOWN"},
        {199, "HOME"},
        {207, "END"},
        {55, "NUMPAD *"},
        {56, "LEFT ALT"},
        {57, "SPACE BAR"},
        {74, "NUMPAD -"},
        {78, "NUMPAD ADD"},
        {83, "NUMPAD DEL"},
        {157, "RIGHT CTRL"},
        {156, "NUMPAD ENTER"},
        {181, "NUMPAD /"},
        {184, "RIGHT ALT"},
    }};
    for (const auto& [key, name] : names)
        if (key == code)
            return name;
    return "";
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00487374
inline std::string_view frontend_joystick_name(int code) {
    constexpr std::array<std::pair<int, std::string_view>, 26> names{{
        {0, "JOYX POSITIVE"},  {1, "JOYY POSITIVE"},  {2, "JOYZ POSITIVE"},  {3, "JOYX NEGATIVE"},
        {4, "JOYY NEGATIVE"},  {5, "JOYZ NEGATIVE"},  {6, "JOY BUTTON 1"},   {7, "JOY BUTTON 2"},
        {8, "JOY BUTTON 3"},   {9, "JOY BUTTON 4"},   {10, "JOY BUTTON 5"},  {11, "JOY BUTTON 6"},
        {12, "JOY BUTTON 7"},  {13, "JOY BUTTON 8"},  {14, "JOY BUTTON 9"},  {15, "JOY BUTTON 10"},
        {16, "JOY BUTTON 11"}, {17, "JOY BUTTON 12"}, {18, "JOY BUTTON 13"}, {19, "JOY BUTTON 14"},
        {20, "JOY BUTTON 15"}, {21, "JOY BUTTON 16"}, {22, "JOY POV FWD"},   {23, "JOY POV RIGHT"},
        {24, "JOY POV BACK"},  {25, "JOY POV LEFT"},
    }};
    for (const auto& [key, name] : names)
        if (key == code)
            return name;
    return "";
}
} // namespace roadwars
